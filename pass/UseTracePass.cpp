#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ModRef.h"
#include "llvm/Support/Path.h"

#include <map>
#include <string>

using namespace llvm;

namespace {
// Expand only PHIs. Keep operand occurrences (including equal opcodes), and
// stop a cyclic PHI path without discarding other incoming paths.
void expandOperand(Value *V, SmallVectorImpl<uint8_t> &Opcodes,
                   SmallPtrSetImpl<PHINode *> &Active) {
  auto *I = dyn_cast<Instruction>(V);
  if (!I)
    return; // Constants, arguments, labels and globals are not instructions.
  if (auto *Phi = dyn_cast<PHINode>(I)) {
    if (!Active.insert(Phi).second)
      return;
    for (Value *Incoming : Phi->incoming_values())
      expandOperand(Incoming, Opcodes, Active);
    Active.erase(Phi);
    return;
  }
  if (I->getOpcode() > 255)
    report_fatal_error("trace format supports one-byte LLVM opcodes");
  Opcodes.push_back(static_cast<uint8_t>(I->getOpcode()));
}

void removePureAttributes(Function &F) {
  F.setMemoryEffects(MemoryEffects::unknown());
  for (auto A : {Attribute::NoSync, Attribute::NoFree, Attribute::Speculatable,
                 Attribute::WillReturn})
    F.removeFnAttr(A);
}

struct UseTracePass : PassInfoMixin<UseTracePass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    // This also protects sim.c, start.c and the logger when used via clang.
    if (sys::path::filename(M.getSourceFileName()) != "app.c" ||
        M.getNamedGlobal("__ir_trace_opcodes"))
      return PreservedAnalyses::all();

    SmallVector<Instruction *, 256> Original;
    std::map<unsigned, std::string> Names;
    unsigned PhiUsers = 0;
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      for (Instruction &I : instructions(F)) {
        if (isa<PHINode>(I)) {
          ++PhiUsers;
          continue;
        }
        if (isa<DbgInfoIntrinsic>(I))
          continue;
        // Snapshot before mutation: inserted calls are never traced.
        Original.push_back(&I);
        Names[I.getOpcode()] = I.getOpcodeName();
      }
      removePureAttributes(F);
    }

    LLVMContext &Ctx = M.getContext();
    IRBuilder<> Builder(Ctx);
    auto Logger = M.getOrInsertFunction(
        "__ir_trace", FunctionType::get(Builder.getVoidTy(),
          {Builder.getInt32Ty(), Builder.getPtrTy(), Builder.getInt32Ty()}, false));
    unsigned Edges = 0;
    for (Instruction *User : Original) {
      SmallVector<uint8_t, 16> Pairs;
      for (Use &Operand : User->operands()) {
        SmallVector<uint8_t, 8> Expanded;
        SmallPtrSet<PHINode *, 8> Active;
        expandOperand(Operand.get(), Expanded, Active);
        for (uint8_t Opcode : Expanded) {
          Pairs.push_back(static_cast<uint8_t>(User->getOpcode()));
          Pairs.push_back(Opcode);
          Names[Opcode] = Instruction::getOpcodeName(Opcode);
          ++Edges;
        }
      }
      Builder.SetInsertPoint(User);
      Value *Data = ConstantPointerNull::get(Builder.getPtrTy());
      if (!Pairs.empty()) {
        auto *Init = ConstantDataArray::get(Ctx, Pairs);
        auto *G = new GlobalVariable(M, Init->getType(), true,
                                    GlobalValue::PrivateLinkage, Init, ".ir.pairs");
        G->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        Data = G;
      }
      Builder.CreateCall(Logger, {Builder.getInt32(User->getOpcode()), Data,
                                 Builder.getInt32(Pairs.size())});
      if (auto *Call = dyn_cast<CallBase>(User)) {
        if (auto *Callee = Call->getCalledFunction()) {
          if (!Callee->isDeclaration()) {
            Call->setMemoryEffects(MemoryEffects::unknown());
            for (auto A : {Attribute::NoSync, Attribute::NoFree,
                           Attribute::Speculatable, Attribute::WillReturn})
              Call->removeFnAttr(A);
          }
        }
        // Logging before a musttail call would be legal, but logging its ret
        // would break adjacency. app.c has no musttail/EH instructions.
      }
    }

    std::string Dictionary = "{\n";
    bool First = true;
    for (const auto &Entry : Names) {
      if (!First)
        Dictionary += ",\n";
      First = false;
      Dictionary += "  \"" + std::to_string(Entry.first) + "\": \"" +
                    Entry.second + "\"";
    }
    Dictionary += "\n}\n";
    auto *Init = ConstantDataArray::getString(Ctx, Dictionary);
    new GlobalVariable(M, Init->getType(), true, GlobalValue::ExternalLinkage,
                       Init, "__ir_trace_opcodes");
    if (verifyModule(M, &errs()))
      report_fatal_error("UseTracePass produced invalid IR");
    errs() << "UseTracePass: " << Original.size() << " users, " << Edges
           << " expanded edges, " << PhiUsers << " PHI users skipped\n";
    return PreservedAnalyses::none();
  }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "UseTracePass", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name != "trace-uses")
                    return false;
                  MPM.addPass(UseTracePass());
                  return true;
                });
            PB.registerOptimizerLastEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel) {
                  MPM.addPass(UseTracePass());
                });
          }};
}
