#include "../sim.h"

#include <stdio.h>
#include <stdlib.h>

static int frame_buffer[SIM_Y_SIZE][SIM_X_SIZE];

void simPutPixel(int x, int y, int argb) {
  if (x < 0 || x >= SIM_X_SIZE || y < 0 || y >= SIM_Y_SIZE) {
    fprintf(stderr, "pixel out of bounds: (%d, %d)\n", x, y);
    exit(1);
  }
  frame_buffer[y][x] = argb;
}

void simFlush(void) {
  FILE *output = fopen("preview.ppm", "wb");
  int y;

  if (output == NULL) {
    fprintf(stderr, "cannot create preview.ppm\n");
    exit(1);
  }

  fprintf(output, "P6\n%d %d\n255\n", SIM_X_SIZE, SIM_Y_SIZE);
  for (y = 0; y < SIM_Y_SIZE; y++) {
    int x;
    for (x = 0; x < SIM_X_SIZE; x++) {
      int color = frame_buffer[y][x];
      fputc((color >> 16) & 0xFF, output);
      fputc((color >> 8) & 0xFF, output);
      fputc(color & 0xFF, output);
    }
  }

  fclose(output);
  printf("created preview.ppm\n");
  exit(0);
}

int main(void) {
  app();
  return 1;
}
