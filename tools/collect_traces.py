"""Run the instrumented app; preserve exact commands, hashes and frame checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha256(path):
    return hashlib.file_digest(path.open("rb"), "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=2)
    parser.add_argument("--backend", choices=["sdl", "headless"], required=True)
    args = parser.parse_args()
    tool = subprocess.check_output([os.environ.get("LLVM_CONFIG", "llvm-config"), "--bindir"], text=True).strip()
    version = subprocess.check_output([str(Path(tool) / "clang"), "--version"], text=True)
    for mode in ("O2", "Os"):
        target = args.output / mode
        target.mkdir(parents=True, exist_ok=True)
        prefix = "trace" if args.backend == "sdl" else "trace-headless"
        command = [str((args.build / f"{prefix}-{mode}").resolve())]
        env = dict(os.environ, IR_TRACE_DIR=str(target.resolve()), TRACE_FRAMES=str(args.frames))
        result = subprocess.run(command, env=env, check=True, capture_output=True, text=True)
        (target / "frames.txt").write_text(result.stdout, encoding="utf-8")
        shutil.copyfile(args.build / f"app_{mode}.ll", target / "app.optimized.ll")
        shutil.copyfile(args.build / f"app_{mode}.instrumented.ll", target / "app.instrumented.ll")
        for kind in ("instructions", "uses"):
            sample = subprocess.check_output(
                ["python3", "tools/decode_trace.py", str(target), "--kind", kind, "--limit", "200"], text=True)
            (target / f"{kind}_sample.txt").write_text(sample, encoding="utf-8")
        manifest = {
            "optimization": f"-{mode}", "backend": args.backend,
            "frames": args.frames, "compiler": version,
            "ir_flags": [f"-{mode}", "-fno-vectorize", "-fno-slp-vectorize"],
            "pipeline": ["clang -S -emit-llvm", "opt -passes=trace-uses -verify-each",
                         "clang -O0 -c (no further IR optimization)", "link", "execute"],
            "command": f"IR_TRACE_DIR=statistics/{mode} TRACE_FRAMES={args.frames} "
                       f"build/trace/{prefix}-{mode}",
            "trace_format": {"instructions": "one opcode byte per non-PHI instruction",
                             "uses": "two opcode bytes per User <- Operand event"},
            "phi": "skip PHI users; recursively emit all incoming instruction alternatives",
            "sha256": {str(path): sha256(path) for path in
                       [Path("app.c"), Path("pass/UseTracePass.cpp"), Path("pass/trace_runtime.c"),
                        Path("tests/trace_driver.c"), target / "app.optimized.ll",
                        target / "app.instrumented.ll",
                        target / "instructions.bin.gz", target / "uses.bin.gz"]},
        }
        (target / "run.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(mode, args.backend, result.stdout, end="", flush=True)


if __name__ == "__main__":
    main()
