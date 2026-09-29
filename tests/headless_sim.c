#include "../sim.h"

#include <stdio.h>
#include <stdlib.h>

#define EXPECTED_PIXELS_PER_FRAME (384 * 192 * 2 * 2)

static int frame_count = 0;
static int pixel_count = 0;

void simPutPixel(int x, int y, int argb) {
  (void)argb;

  if (x < 0 || x >= SIM_X_SIZE || y < 0 || y >= SIM_Y_SIZE) {
    fprintf(stderr, "pixel out of bounds: (%d, %d)\n", x, y);
    exit(1);
  }
  pixel_count++;
}

void simFlush(void) {
  if (pixel_count != EXPECTED_PIXELS_PER_FRAME) {
    fprintf(stderr, "unexpected pixel count: %d\n", pixel_count);
    exit(1);
  }

  frame_count++;
  pixel_count = 0;
  if (frame_count == 2) {
    printf("headless smoke test passed: %d frames\n", frame_count);
    exit(0);
  }
}

int main(void) {
  app();
  return 1;
}
