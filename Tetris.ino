#include <Arduino.h>
#include "display.h"
#include "input.h"
#include "game.h"
#include "conway.h"
#include "stacker.h"
#include "space_invaders.h"

#define SCREENSAVER_TIMEOUT_MS 30000UL

static const Game *games[] = {
    &stacker_game,
    &si_game,
};
#define NUM_GAMES (int)(sizeof(games) / sizeof(games[0]))

static int current_game   = 0;
static int in_screensaver = 0;

void setup() {
    /* Seed the C rand() used by Conway and Space Invaders with floating-
       pin noise — analogRead on an unconnected pin gives random values. */
    srand((unsigned)analogRead(A0));

    display_init();
    input_init();
    games[current_game]->init();
}

void loop() {
    InputEvent    ev  = input_poll();
    unsigned long now = millis();

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
}
