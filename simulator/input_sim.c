#include <SDL2/SDL.h>
#include "../input.h"
#include "input_sim.h"

/* Keyboard mapping:
     Space  → INPUT_TAP
     D      → INPUT_DOUBLE_TAP
     H      → INPUT_HOLD
     Q / close window → quit flag */

static unsigned long last_event_ms = 0;
static int           should_quit   = 0;

void input_init(void) {
    SDL_Init(SDL_INIT_EVENTS);
    /* Drain stale events so old input doesn't bleed into the new game state. */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {}
    last_event_ms = 0;
    should_quit   = 0;
}

InputEvent input_poll(void) {
    SDL_Event  event;
    InputEvent result = INPUT_NONE;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            should_quit = 1;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:     should_quit = 1;            break;
            case SDLK_SPACE: result = INPUT_TAP;          break;
            case SDLK_d:     result = INPUT_DOUBLE_TAP;   break;
            case SDLK_h:     result = INPUT_HOLD;         break;
            default: break;
            }
            break;
        default:
            break;
        }
        if (result != INPUT_NONE) break; /* take the first mappable event per frame */
    }

    if (result != INPUT_NONE)
        last_event_ms = (unsigned long)SDL_GetTicks();

    return result;
}

unsigned long input_last_event_ms(void) {
    return last_event_ms;
}

int input_sim_should_quit(void) {
    return should_quit;
}
