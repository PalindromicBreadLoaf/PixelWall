#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include "../display.h"
#include "../input.h"
#include "../game.h"
#include "../conway.h"
#include "../stacker.h"
#include "../space_invaders.h"
#include "input_sim.h"

extern void display_sim_quit(void);

#define SCREENSAVER_TIMEOUT_MS 30000UL
#define WIPE_STEP_MS              30UL  /* ms per column — 10 columns = 300 ms total */

static const Game *games[] = {
    &stacker_game,
    &si_game,
};
#define NUM_GAMES ((int)(sizeof(games) / sizeof(games[0])))

int main(void) {
    srand((unsigned)time(NULL));

    display_init();
    input_init();

    int current_game  = 0;
    int in_screensaver = 0;
    int in_wipe       = 0;
    int wipe_out      = 0;
    int wipe_next     = 0;
    int wipe_col      = 0;
    unsigned long last_wipe_ms = 0;

    games[current_game]->init();

    while (!input_sim_should_quit()) {
        InputEvent ev = input_poll();
        unsigned long now = (unsigned long)SDL_GetTicks();

        if (ev != INPUT_NONE) {
            if (in_screensaver) {
                in_screensaver = 0;
                games[current_game]->init();
            } else if (ev == INPUT_HOLD) {
                wipe_out     = 0;
                wipe_next    = (current_game + 1) % NUM_GAMES;
                wipe_col     = 0;
                in_wipe      = 1;
                last_wipe_ms = now;
            } else {
                games[current_game]->on_input(ev);
            }
        }

        if (!in_screensaver
                && input_last_event_ms() != 0
                && now - input_last_event_ms() > SCREENSAVER_TIMEOUT_MS) {
            in_screensaver = 1;
            conway_init();
        }

        if (in_wipe) {
            if (!wipe_out) {
                /* WIPE_IN: paint one white column per step, left → right. */
                if (now - last_wipe_ms >= WIPE_STEP_MS) {
                    last_wipe_ms = now;
                    for (int y = 0; y < DISPLAY_H; y++)
                        display_set((uint8_t)wipe_col, (uint8_t)y, 255, 255, 255);
                    wipe_col++;
                    if (wipe_col >= DISPLAY_W) {
                        wipe_out     = 1;
                        wipe_col     = 0;
                        current_game = wipe_next;
                        games[current_game]->init();
                        last_wipe_ms = now;
                        /* Repaint white so init()'s draw doesn't show this frame. */
                        for (int x = 0; x < DISPLAY_W; x++)
                            for (int y = 0; y < DISPLAY_H; y++)
                                display_set((uint8_t)x, (uint8_t)y, 255, 255, 255);
                    }
                }
            } else {
                /* WIPE_OUT: run the new game each frame so revealed columns
                   show a live image, then re-paint white over the unrevealed
                   right-side columns on top. */
                games[current_game]->update(now);
                for (int col = wipe_col; col < DISPLAY_W; col++)
                    for (int y = 0; y < DISPLAY_H; y++)
                        display_set((uint8_t)col, (uint8_t)y, 255, 255, 255);
                if (now - last_wipe_ms >= WIPE_STEP_MS) {
                    last_wipe_ms = now;
                    wipe_col++;
                    if (wipe_col >= DISPLAY_W)
                        in_wipe = 0;
                }
            }
        } else if (in_screensaver) {
            conway_update(now);
        } else {
            games[current_game]->update(now);
            if (games[current_game]->is_over())
                games[current_game]->init();
        }

        display_show();
        SDL_Delay(16);
    }

    display_sim_quit();
    return 0;
}
