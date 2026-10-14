CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
SDL_LIBS := -lSDL2
IR_FLAGS := -S -emit-llvm -fno-vectorize -fno-slp-vectorize

.PHONY: all run ir test preview clean

all: julia

julia: start.c sim.c app.c sim.h
	$(CC) $(CFLAGS) start.c sim.c app.c $(SDL_LIBS) -o $@

run: julia
	./julia

app_O2.ll: app.c sim.h
	$(CC) $(CFLAGS) $(IR_FLAGS) -O2 app.c -o $@

app_Os.ll: app.c sim.h
	$(CC) $(CFLAGS) $(IR_FLAGS) -Os app.c -o $@

ir: app_O2.ll app_Os.ll

julia-ir: start.c sim.c app_O2.ll sim.h
	$(CC) $(CFLAGS) start.c sim.c app_O2.ll $(SDL_LIBS) -o $@

headless-test: app.c tests/headless_sim.c sim.h
	$(CC) $(CFLAGS) app.c tests/headless_sim.c -o $@

test: headless-test
	./headless-test

preview-dump: app.c tests/frame_dump.c sim.h
	$(CC) $(CFLAGS) app.c tests/frame_dump.c -o $@

preview: preview-dump
	./preview-dump

clean:
	rm -f julia julia-ir headless-test headless-ubsan preview-dump preview.ppm app_O2.ll app_Os.ll

include pass/targets.mk
