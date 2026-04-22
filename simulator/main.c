#include <SDL2/SDL.h>
#include "../display.h"

extern void display_sim_quit(void);

int main(void) {
    display_init();

    /* Draw a test pattern: horizontal rainbow gradient top-to-bottom,
       with brightness increasing left-to-right, so every panel is visible. */
    for (int y = 0; y < DISPLAY_H; y++) {
        for (int x = 0; x < DISPLAY_W; x++) {
            uint8_t r = (uint8_t)((x * 255) / (DISPLAY_W - 1));
            uint8_t g = (uint8_t)((y * 255) / (DISPLAY_H - 1));
            uint8_t b = 80;
            display_set((uint8_t)x, (uint8_t)y, r, g, b);
        }
    }
    display_show();

    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q) running = 0;
        }
        SDL_Delay(16);
    }

    display_sim_quit();
    return 0;
}
