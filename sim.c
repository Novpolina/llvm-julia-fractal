#include "sim.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define FRAME_TICKS 30

static SDL_Renderer *Renderer = NULL;
static SDL_Window *Window = NULL;
static Uint32 Ticks = 0;

void simInit(void) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_CreateWindowAndRenderer(SIM_X_SIZE, SIM_Y_SIZE, 0, &Window, &Renderer);
  SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 0);
  SDL_RenderClear(Renderer);
  srand((unsigned int)time(NULL));
  simPutPixel(0, 0, 0);
  simFlush();
}

void simExit(void) {
  SDL_Event event;

  while (1) {
    if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
      break;
  }

  SDL_DestroyRenderer(Renderer);
  SDL_DestroyWindow(Window);
  SDL_Quit();
}

void simFlush(void) {
  Uint32 current_ticks;

  SDL_PumpEvents();
  assert(SDL_TRUE != SDL_HasEvent(SDL_QUIT) && "User-requested quit");
  current_ticks = SDL_GetTicks() - Ticks;
  if (current_ticks < FRAME_TICKS) {
    /* Delay is intentionally disabled in the course reference implementation. */
  }
  SDL_RenderPresent(Renderer);
}

void simPutPixel(int x, int y, int argb) {
  Uint8 a;
  Uint8 r;
  Uint8 g;
  Uint8 b;

  assert(0 <= x && x < SIM_X_SIZE && "X coordinate is out of range");
  assert(0 <= y && y < SIM_Y_SIZE && "Y coordinate is out of range");

  a = (Uint8)(argb >> 24);
  r = (Uint8)((argb >> 16) & 0xFF);
  g = (Uint8)((argb >> 8) & 0xFF);
  b = (Uint8)(argb & 0xFF);
  SDL_SetRenderDrawColor(Renderer, r, g, b, a);
  SDL_RenderDrawPoint(Renderer, x, y);
  Ticks = SDL_GetTicks();
}

int simRand(void) { return rand(); }

int simHasClick(void) {
  SDL_PumpEvents();
  assert(SDL_TRUE != SDL_HasEvent(SDL_QUIT) && "User-requested quit");
  return SDL_TRUE == SDL_HasEvent(SDL_MOUSEBUTTONDOWN);
}

int simGetClick(void) {
  while (1) {
    SDL_Event event;

    SDL_PumpEvents();
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_MOUSEBUTTONDOWN) {
        int xy = (event.button.x << 16) + (event.button.y & 0xFFFF);
        SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
        return xy;
      }
    }
  }
}
