# LLVM's tools and plugin must have the same major version.
LLVM_CONFIG ?= llvm-config
LLVM_BIN := $(shell $(LLVM_CONFIG) --bindir)
TRACE_CC := $(LLVM_BIN)/clang
TRACE_CXX := $(LLVM_BIN)/clang++
TRACE_OPT := $(LLVM_BIN)/opt
TRACE_BUILD := build/trace
TRACE_FRAMES ?= 2
export LLVM_CONFIG

.PHONY: pass trace trace-headless trace-test trace-analysis

$(TRACE_BUILD):
	mkdir -p $@

$(TRACE_BUILD)/libUseTracePass.so: pass/UseTracePass.cpp | $(TRACE_BUILD)
	$(TRACE_CXX) $(shell $(LLVM_CONFIG) --cxxflags) -O2 -fPIC -shared $< -o $@ $(shell $(LLVM_CONFIG) --ldflags --libs core passes --system-libs)

pass: $(TRACE_BUILD)/libUseTracePass.so

$(TRACE_BUILD)/app_O2.ll $(TRACE_BUILD)/app_Os.ll: $(TRACE_BUILD)/app_%.ll: app.c sim.h | $(TRACE_BUILD)
	$(TRACE_CC) $(CFLAGS) $(IR_FLAGS) -$* app.c -o $@

$(TRACE_BUILD)/app_%.instrumented.ll: $(TRACE_BUILD)/app_%.ll $(TRACE_BUILD)/libUseTracePass.so
	$(TRACE_OPT) -load-pass-plugin=$(TRACE_BUILD)/libUseTracePass.so -passes=trace-uses -verify-each -S $< -o $@

# No optimization pipeline after instrumentation: lower the recorded IR as-is.
$(TRACE_BUILD)/app_%.o: $(TRACE_BUILD)/app_%.instrumented.ll
	$(TRACE_CC) -O0 -c $< -o $@

$(TRACE_BUILD)/runtime.o: pass/trace_runtime.c | $(TRACE_BUILD)
	$(TRACE_CC) $(CFLAGS) -O2 -c $< -o $@

$(TRACE_BUILD)/trace-%: $(TRACE_BUILD)/app_%.o $(TRACE_BUILD)/runtime.o tests/trace_driver.c sim.c sim.h
	$(TRACE_CC) $(CFLAGS) -O2 tests/trace_driver.c sim.c $(TRACE_BUILD)/app_$*.o $(TRACE_BUILD)/runtime.o $(SDL_LIBS) -lz -Wl,--wrap=simFlush,--wrap=simPutPixel -o $@

$(TRACE_BUILD)/trace-headless-%: $(TRACE_BUILD)/app_%.o $(TRACE_BUILD)/runtime.o tests/trace_driver.c sim.h
	$(TRACE_CC) $(CFLAGS) -O2 -DTRACE_HEADLESS tests/trace_driver.c $(TRACE_BUILD)/app_$*.o $(TRACE_BUILD)/runtime.o -lz -o $@

$(TRACE_BUILD)/baseline-%: $(TRACE_BUILD)/app_%.ll tests/trace_driver.c sim.h
	$(TRACE_CC) $(CFLAGS) -O0 -DTRACE_HEADLESS tests/trace_driver.c $< -o $@

$(TRACE_BUILD)/analyze-trace: tools/analyze_trace.cpp | $(TRACE_BUILD)
	$(TRACE_CXX) -std=c++17 -O2 -Wall -Wextra -Wpedantic $< -lz -o $@

trace: $(TRACE_BUILD)/trace-O2 $(TRACE_BUILD)/trace-Os
	python3 tools/collect_traces.py --build $(TRACE_BUILD) --output statistics --frames $(TRACE_FRAMES) --backend sdl

trace-headless: $(TRACE_BUILD)/trace-headless-O2 $(TRACE_BUILD)/trace-headless-Os
	python3 tools/collect_traces.py --build $(TRACE_BUILD) --output statistics --frames $(TRACE_FRAMES) --backend headless

trace-analysis: $(TRACE_BUILD)/analyze-trace
	$(TRACE_BUILD)/analyze-trace statistics/O2
	$(TRACE_BUILD)/analyze-trace statistics/Os
	python3 tools/report_trace.py statistics

trace-test: pass $(TRACE_BUILD)/trace-headless-O2 $(TRACE_BUILD)/trace-headless-Os $(TRACE_BUILD)/baseline-O2 $(TRACE_BUILD)/baseline-Os $(TRACE_BUILD)/analyze-trace
	python3 tests/test_trace.py --build $(TRACE_BUILD)

.SECONDARY:
