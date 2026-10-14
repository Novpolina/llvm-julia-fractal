#include "../sim.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PIXELS_PER_FRAME (384u * 192u * 4u)
static unsigned pixels;
static unsigned frames;
static unsigned frame_limit = 2;
static uint64_t checksum = UINT64_C(14695981039346656037);

#ifndef TRACE_HEADLESS
void __real_simPutPixel(int x, int y, int argb);
void __real_simFlush(void);
static int active;
#define PIXEL_CALLBACK __wrap_simPutPixel
#define FLUSH_CALLBACK __wrap_simFlush
#else
#define PIXEL_CALLBACK simPutPixel
#define FLUSH_CALLBACK simFlush
#endif

static void hash_word(uint32_t value) {
  for (unsigned byte = 0; byte < 4; ++byte) {
    checksum ^= (value >> (8 * byte)) & 255u;
    checksum *= UINT64_C(1099511628211);
  }
}

void PIXEL_CALLBACK(int x, int y, int argb) {
#ifndef TRACE_HEADLESS
  __real_simPutPixel(x, y, argb);
  if (!active)
    return;
#endif
  if (x < 0 || x >= SIM_X_SIZE || y < 0 || y >= SIM_Y_SIZE) {
    fprintf(stderr, "pixel outside framebuffer\n");
    exit(1);
  }
  ++pixels;
  hash_word((uint32_t)x);
  hash_word((uint32_t)y);
  hash_word((uint32_t)argb);
}

void FLUSH_CALLBACK(void) {
#ifndef TRACE_HEADLESS
  __real_simFlush();
  if (!active)
    return;
#endif
  if (pixels != PIXELS_PER_FRAME) {
    fprintf(stderr, "incomplete frame: %u pixels\n", pixels);
    exit(1);
  }
  printf("frame=%u pixels=%u fnv1a64=%016" PRIx64 "\n", frames, pixels, checksum);
  pixels = 0;
  checksum = UINT64_C(14695981039346656037);
  if (++frames == frame_limit) {
#ifndef TRACE_HEADLESS
    /* The course simExit waits for an event; destroy SDL resources directly. */
    extern void SDL_Quit(void);
    SDL_Quit();
#endif
    exit(0); // atexit flushes the complete runtime trace.
  }
}

int main(void) {
  const char *limit = getenv("TRACE_FRAMES");
  if (limit) {
    char *end;
    unsigned long parsed = strtoul(limit, &end, 10);
    if (!*limit || *end || parsed < 1 || parsed > 1000000) {
      fprintf(stderr, "TRACE_FRAMES must be 1..1000000\n");
      return 1;
    }
    frame_limit = (unsigned)parsed;
  }
#ifndef TRACE_HEADLESS
  simInit();
  active = 1;
#endif
  app();
  return 1;
}
