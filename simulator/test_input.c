#include <SDL2/SDL.h>
#include <stdio.h>
#include "../input.h"
#include "input_sim.h"

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do {                                    \
    tests_run++;                                                  \
    if (cond) {                                                   \
        tests_passed++;                                           \
        printf("  PASS  %s\n", msg);                             \
    } else {                                                      \
        tests_failed++;                                           \
        printf("  FAIL  %s  (line %d)\n", msg, __LINE__);        \
    }                                                             \
} while (0)

/* Push a synthetic KEYDOWN event into the SDL queue. */
static void push_key(SDL_Keycode sym) {
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type           = SDL_KEYDOWN;
    e.key.keysym.sym = sym;
    SDL_PushEvent(&e);
}

/* Push a synthetic SDL_QUIT event. */
static void push_quit(void) {
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
}

/* ------------------------------------------------------------------ */

static void test_key_mapping(void) {
    puts("key_mapping");
    InputEvent ev;

    input_init();
    push_key(SDLK_SPACE);
    ev = input_poll();
    ASSERT(ev == INPUT_TAP, "space → INPUT_TAP");

    input_init();
    push_key(SDLK_d);
    ev = input_poll();
    ASSERT(ev == INPUT_DOUBLE_TAP, "d → INPUT_DOUBLE_TAP");

    input_init();
    push_key(SDLK_h);
    ev = input_poll();
    ASSERT(ev == INPUT_HOLD, "h → INPUT_HOLD");

    input_init();
    ev = input_poll();
    ASSERT(ev == INPUT_NONE, "no key → INPUT_NONE");
}

static void test_unknown_keys_ignored(void) {
    puts("unknown_keys_ignored");

    input_init();
    push_key(SDLK_a);
    push_key(SDLK_RETURN);
    push_key(SDLK_ESCAPE);
    InputEvent ev = input_poll();
    ASSERT(ev == INPUT_NONE, "unmapped keys produce INPUT_NONE");
}

static void test_last_event_ms(void) {
    puts("last_event_ms");

    input_init();
    ASSERT(input_last_event_ms() == 0, "last_event_ms is 0 after init");

    /* Delay once so SDL_GetTicks() advances past 0. */
    SDL_Delay(2);

    push_key(SDLK_SPACE);
    input_poll();
    unsigned long after_tap = input_last_event_ms();
    ASSERT(after_tap > 0, "last_event_ms > 0 after TAP");

    /* NONE must not change the timestamp. */
    input_poll(); /* nothing queued */
    ASSERT(input_last_event_ms() == after_tap,
           "last_event_ms unchanged when INPUT_NONE returned");

    /* All three event types must record a non-decreasing timestamp.
       We use >= rather than > because two SDL_GetTicks() calls within
       the same millisecond may return the same value. */
    push_key(SDLK_d);
    input_poll();
    unsigned long after_double = input_last_event_ms();
    ASSERT(after_double >= after_tap, "last_event_ms non-decreasing on DOUBLE_TAP");

    push_key(SDLK_h);
    input_poll();
    unsigned long after_hold = input_last_event_ms();
    ASSERT(after_hold >= after_double, "last_event_ms non-decreasing on HOLD");
}

static void test_quit_flag(void) {
    puts("quit_flag");

    input_init();
    ASSERT(!input_sim_should_quit(), "should_quit is 0 after init");

    push_quit();
    input_poll();
    ASSERT(input_sim_should_quit(), "should_quit set on SDL_QUIT event");

    input_init();
    ASSERT(!input_sim_should_quit(), "should_quit reset after reinit");

    push_key(SDLK_q);
    input_poll();
    ASSERT(input_sim_should_quit(), "should_quit set on Q key");
}

static void test_first_event_in_frame_wins(void) {
    puts("first_event_in_frame_wins");

    /* If multiple mappable keys are queued, the first one is returned.
       The second is consumed but lost (acceptable single-button hardware model). */
    input_init();
    push_key(SDLK_SPACE);
    push_key(SDLK_h);
    InputEvent ev = input_poll();
    ASSERT(ev == INPUT_TAP, "first queued key wins (space before h)");
}

/* ------------------------------------------------------------------ */

int main(void) {
    SDL_Init(SDL_INIT_EVENTS);

    puts("=== input tests ===");
    test_key_mapping();
    test_unknown_keys_ignored();
    test_last_event_ms();
    test_quit_flag();
    test_first_event_in_frame_wins();

    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');

    SDL_Quit();
    return tests_failed ? 1 : 0;
}
