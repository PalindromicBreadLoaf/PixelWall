#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include "../display.h"
#include "../input.h"
#include "../game.h"
#include "../conway.h"
#include "../stacker.h"
#include "input_sim.h"

extern void display_sim_quit(void);

#define SCREENSAVER_TIMEOUT_MS 30000UL

static const Game *games[] = {
    &stacker_game,
};
#define NUM_GAMES ((int)(sizeof(games) / sizeof(games[0])))

int main(void) {
    srand((unsigned)time(NULL));

    display_init();
    input_init();

    int current_game  = 0;
    int in_screensaver = 0;

    games[current_game]->init();

    while (!input_sim_should_quit()) {
        InputEvent ev = input_poll();
        unsigned long now = (unsigned long)SDL_GetTicks();

        if (ev != INPUT_NONE) {
            if (in_screensaver) {
                in_screensaver = 0;
                games[current_game]->init();
            } else if (ev == INPUT_HOLD) {
                current_game = (current_game + 1) % NUM_GAMES;
                games[current_game]->init();
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

        if (in_screensaver) {
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
