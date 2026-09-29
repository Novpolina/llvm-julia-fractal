#include "sim.h"

#define GRID_X 384
#define GRID_Y 192
#define CELL_SIZE 4
#define DOT_SIZE 2
#define DOT_OFFSET 1

#define FIXED_SHIFT 12
#define FIXED_ONE (1 << FIXED_SHIFT)
#define ESCAPE_LIMIT (2 * FIXED_ONE)
#define ESCAPE_RADIUS_SQUARED (4 * FIXED_ONE * FIXED_ONE)

#define VIEW_X_MIN (-7373)
#define VIEW_X_SPAN 14746
#define VIEW_Y_MIN (-3686)
#define VIEW_Y_SPAN 7372

#define MAX_ITERATIONS 48
#define FRAME_LIMIT 1000000

int triangleWave(int phase, int period, int amplitude) {
  int half;
  int position;

  phase %= period;
  if (phase < 0)
    phase += period;

  half = period / 2;
  position = phase < half ? phase : period - phase;
  return position * 4 * amplitude / period - amplitude;
}

int calculateIterations(int x, int y, int c_re, int c_im) {
  int z_re = x;
  int z_im = y;
  int iteration = 0;

  while (iteration < MAX_ITERATIONS) {
    int re_squared;
    int im_squared;
    int next_re;
    int next_im;

    if (z_re > ESCAPE_LIMIT || z_re < -ESCAPE_LIMIT ||
        z_im > ESCAPE_LIMIT || z_im < -ESCAPE_LIMIT)
      break;

    re_squared = z_re * z_re;
    im_squared = z_im * z_im;
    if (re_squared + im_squared > ESCAPE_RADIUS_SQUARED)
      break;

    next_re = (re_squared - im_squared) / FIXED_ONE + c_re;
    next_im = (2 * z_re * z_im) / FIXED_ONE + c_im;
    z_re = next_re;
    z_im = next_im;
    iteration++;
  }

  return iteration;
}

int colorWave(int phase) {
  phase &= 255;
  if (phase < 128)
    return phase * 2;
  return (255 - phase) * 2;
}

int makeColor(int iterations, int frame) {
  int phase;
  int brightness;
  int red;
  int green;
  int blue;

  if (iterations == MAX_ITERATIONS)
    return 0xFF030008;

  phase = (iterations * 9 + frame) & 255;
  brightness = 48 + iterations * 207 / MAX_ITERATIONS;
  red = colorWave(phase) * brightness / 255;
  green = colorWave(phase + 85) * brightness / 255;
  blue = colorWave(phase + 170) * brightness / 255;
  return 0xFF000000 | (red << 16) | (green << 8) | blue;
}

void drawCell(int grid_x, int grid_y, int color) {
  int origin_x = grid_x * CELL_SIZE + DOT_OFFSET;
  int origin_y = grid_y * CELL_SIZE + DOT_OFFSET;
  int dy;

  for (dy = 0; dy < DOT_SIZE; dy++) {
    int dx;
    for (dx = 0; dx < DOT_SIZE; dx++)
      simPutPixel(origin_x + dx, origin_y + dy, color);
  }
}

void app(void) {
  int frame = 0;

  while (1) {
    int c_re = -3210 + triangleWave(frame, 600, 220);
    int c_im = 640 + triangleWave(frame + 170, 840, 260);
    int pixel_y;

    for (pixel_y = 0; pixel_y < GRID_Y; pixel_y++) {
      int y = VIEW_Y_MIN + pixel_y * VIEW_Y_SPAN / (GRID_Y - 1);
      int pixel_x;

      for (pixel_x = 0; pixel_x < GRID_X; pixel_x++) {
        int x = VIEW_X_MIN + pixel_x * VIEW_X_SPAN / (GRID_X - 1);
        int iterations = calculateIterations(x, y, c_re, c_im);
        int color = makeColor(iterations, frame / 3);
        drawCell(pixel_x, pixel_y, color);
      }
    }

    simFlush();
    frame++;
    if (frame == FRAME_LIMIT)
      frame = 0;
  }
}
