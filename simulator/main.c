#include <SDL2/SDL.h>
#include "../display.h"
#include "../input.h"
#include "input_sim.h"

extern void display_sim_quit(void);

int main(void) {
    display_init();
    input_init();

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

    while (!input_sim_should_quit()) {
        input_poll();
        /* Game loop will go here. */
        SDL_Delay(16);
    }

    display_sim_quit();
    return 0;
}
