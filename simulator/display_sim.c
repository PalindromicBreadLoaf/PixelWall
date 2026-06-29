#include <SDL2/SDL.h>
#include "display_buf.h"

#define PIXEL_SIZE 20
#define PIXEL_GAP   3
#define PANEL_GAP  10
#define PANEL_SIZE  5

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;

// Add wider gaps between where physical panels would be.
static int to_screen(int logical) {
    return logical * (PIXEL_SIZE + PIXEL_GAP)
         + (logical / PANEL_SIZE) * (PANEL_GAP - PIXEL_GAP);
}

static int window_dimension(int logical_size) {
    return to_screen(logical_size - 1) + PIXEL_SIZE;
}

void display_init(void) {
    display_buf_init();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    int w = window_dimension(DISPLAY_W);
    int h = window_dimension(DISPLAY_H);

    window = SDL_CreateWindow(
        "Pixel Wall Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, 0
    );
    renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
}

void display_sim_quit(void) {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();
}

void display_show(void) {
    SDL_SetRenderDrawColor(renderer, 0x11, 0x11, 0x11, 0xFF);
    SDL_RenderClear(renderer);

    for (int y = 0; y < DISPLAY_H; y++) {
        for (int x = 0; x < DISPLAY_W; x++) {
            uint8_t r = display_fb[y][x][0];
            uint8_t g = display_fb[y][x][1];
            uint8_t b = display_fb[y][x][2];

            SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);
            SDL_Rect rect = { to_screen(x), to_screen(y), PIXEL_SIZE, PIXEL_SIZE };
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    SDL_RenderPresent(renderer);
}
