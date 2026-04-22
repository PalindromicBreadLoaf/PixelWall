#include <SDL2/SDL.h>
#include "../input.h"
#include "input_sim.h"

/* Space bar implements real press-and-hold detection using SDL event timestamps:
     short press  (<TAP_MAX_MS)                    → INPUT_TAP
     second short press within DOUBLE_TAP_WIN_MS   → INPUT_DOUBLE_TAP
     press held >= hold_min_ms (while still down)  → INPUT_HOLD

   H and D remain instant shortcuts for convenience during development.
   Q / window close → quit.

   Event timestamps (event.key.timestamp) are used for press/release durations
   so that events queued before a delay correctly measure elapsed time.
   SDL_GetTicks() is used only for the per-frame hold check (current real time). */

#define TAP_MAX_MS        300UL
#define DOUBLE_TAP_WIN_MS 400UL
#define HOLD_MIN_MS_DEFAULT 2000UL

typedef enum { ST_IDLE, ST_PRESSING, ST_AFTER_TAP, ST_HELD } BtnState;

static BtnState      btn_state      = ST_IDLE;
static unsigned long press_start_ms = 0;   /* event.key.timestamp of last KEYDOWN */
static unsigned long tap_release_ms = 0;   /* event.key.timestamp of first KEYUP */
static int           first_tap_done = 0;   /* 1 when re-pressing during AFTER_TAP */
static unsigned long hold_min_ms    = HOLD_MIN_MS_DEFAULT;

static unsigned long last_event_ms_val = 0;
static int           should_quit       = 0;

/* ------------------------------------------------------------------ */

void input_init(void) {
    SDL_Init(SDL_INIT_EVENTS);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {}  /* drain stale events */
    btn_state          = ST_IDLE;
    press_start_ms     = 0;
    tap_release_ms     = 0;
    first_tap_done     = 0;
    hold_min_ms        = HOLD_MIN_MS_DEFAULT;
    last_event_ms_val  = 0;
    should_quit        = 0;
}

void input_sim_set_hold_min_ms(unsigned long ms) {
    hold_min_ms = ms;
}

InputEvent input_poll(void) {
    InputEvent result = INPUT_NONE;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        unsigned long evt_time = (unsigned long)event.key.timestamp;

        switch (event.type) {

        case SDL_QUIT:
            should_quit = 1;
            break;

        case SDL_KEYDOWN:
            if (event.key.repeat) break;
            switch (event.key.keysym.sym) {
            case SDLK_q:
                should_quit = 1;
                break;
            case SDLK_SPACE:
                if (btn_state == ST_IDLE) {
                    btn_state      = ST_PRESSING;
                    press_start_ms = evt_time;
                    first_tap_done = 0;
                } else if (btn_state == ST_AFTER_TAP) {
                    btn_state      = ST_PRESSING;
                    press_start_ms = evt_time;
                    first_tap_done = 1;
                }
                break;
            case SDLK_h:
                result = INPUT_HOLD;
                break;
            case SDLK_d:
                result = INPUT_DOUBLE_TAP;
                break;
            default:
                break;
            }
            break;

        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_SPACE) {
                if (btn_state == ST_PRESSING) {
                    unsigned long dur = evt_time - press_start_ms;
                    if (dur < TAP_MAX_MS) {
                        if (first_tap_done) {
                            result         = INPUT_DOUBLE_TAP;
                            btn_state      = ST_IDLE;
                            first_tap_done = 0;
                        } else {
                            result         = INPUT_TAP;
                            btn_state      = ST_AFTER_TAP;
                            tap_release_ms = evt_time;
                        }
                    } else if (dur >= hold_min_ms) {
                        /* Released after hold threshold before per-frame check fired. */
                        result         = INPUT_HOLD;
                        btn_state      = ST_IDLE;
                        first_tap_done = 0;
                    } else {
                        /* Sloppy press: too long for tap, too short for hold. */
                        btn_state      = ST_IDLE;
                        first_tap_done = 0;
                    }
                } else if (btn_state == ST_HELD) {
                    btn_state = ST_IDLE;
                }
            }
            break;

        default:
            break;
        }

        if (result != INPUT_NONE) break;
    }

    /* Per-frame hold check using current real time. Fires while Space is held. */
    if (result == INPUT_NONE && btn_state == ST_PRESSING) {
        unsigned long now = (unsigned long)SDL_GetTicks();
        if (now - press_start_ms >= hold_min_ms) {
            result         = INPUT_HOLD;
            btn_state      = ST_HELD;
            first_tap_done = 0;
        }
    }

    /* Double-tap window expiry. */
    if (btn_state == ST_AFTER_TAP) {
        unsigned long now = (unsigned long)SDL_GetTicks();
        if (now - tap_release_ms >= DOUBLE_TAP_WIN_MS)
            btn_state = ST_IDLE;
    }

    if (result != INPUT_NONE)
        last_event_ms_val = (unsigned long)SDL_GetTicks();

    return result;
}

unsigned long input_last_event_ms(void) {
    return last_event_ms_val;
}

int input_sim_should_quit(void) {
    return should_quit;
}
