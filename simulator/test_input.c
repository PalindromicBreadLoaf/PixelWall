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

static void push_keydown(SDL_Keycode sym) {
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type              = SDL_KEYDOWN;
    e.key.timestamp     = SDL_GetTicks();
    e.key.keysym.sym    = sym;
    SDL_PushEvent(&e);
}

static void push_keyup(SDL_Keycode sym) {
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type              = SDL_KEYUP;
    e.key.timestamp     = SDL_GetTicks();
    e.key.keysym.sym    = sym;
    SDL_PushEvent(&e);
}

static void push_quit(void) {
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
}

/* ------------------------------------------------------------------ */

static void test_tap_on_quick_press(void) {
    puts("tap_on_quick_press");

    /* Space down then up in the same poll → duration ≈ 0 < TAP_MAX_MS → TAP */
    input_init();
    push_keydown(SDLK_SPACE);
    push_keyup(SDLK_SPACE);
    ASSERT(input_poll() == INPUT_TAP, "space down+up → INPUT_TAP");

    /* Key repeat events must be ignored. */
    input_init();
    SDL_Event repeat;
    SDL_memset(&repeat, 0, sizeof(repeat));
    repeat.type        = SDL_KEYDOWN;
    repeat.key.repeat  = 1;
    repeat.key.keysym.sym = SDLK_SPACE;
    SDL_PushEvent(&repeat);
    ASSERT(input_poll() == INPUT_NONE, "auto-repeat KEYDOWN ignored");
}

static void test_double_tap(void) {
    puts("double_tap");

    input_init();

    /* Push all four events at once so they share the same timestamp range.
       First poll takes down1+up1 and breaks (result=TAP); down2+up2 stay queued.
       Second poll finds btn_state=ST_AFTER_TAP, processes down2+up2 → DOUBLE_TAP.
       This avoids any wall-clock race with the double-tap window expiry. */
    push_keydown(SDLK_SPACE);
    push_keyup(SDLK_SPACE);
    push_keydown(SDLK_SPACE);
    push_keyup(SDLK_SPACE);

    ASSERT(input_poll() == INPUT_TAP,        "first poll → INPUT_TAP");
    ASSERT(input_poll() == INPUT_DOUBLE_TAP, "second poll → INPUT_DOUBLE_TAP");
}

static void test_hold_via_space(void) {
    puts("hold_via_space");

    /* Use a short threshold so the test doesn't wait 2 seconds. */
    input_init();
    input_sim_set_hold_min_ms(50);

    push_keydown(SDLK_SPACE);
    SDL_Delay(80);             /* wait longer than 50 ms */
    ASSERT(input_poll() == INPUT_HOLD, "space held >= hold_min_ms → INPUT_HOLD");

    /* Release the key to clean up state. */
    push_keyup(SDLK_SPACE);
    input_poll();
}

static void test_hold_shortcuts(void) {
    puts("hold_shortcuts");

    /* H key → instant HOLD (no press duration required). */
    input_init();
    push_keydown(SDLK_h);
    ASSERT(input_poll() == INPUT_HOLD, "H key → instant INPUT_HOLD");

    /* D key → instant DOUBLE_TAP. */
    input_init();
    push_keydown(SDLK_d);
    ASSERT(input_poll() == INPUT_DOUBLE_TAP, "D key → instant INPUT_DOUBLE_TAP");
}

static void test_unknown_keys_ignored(void) {
    puts("unknown_keys_ignored");

    input_init();
    push_keydown(SDLK_a);
    push_keydown(SDLK_RETURN);
    push_keydown(SDLK_ESCAPE);
    ASSERT(input_poll() == INPUT_NONE, "unmapped keys produce INPUT_NONE");
}

static void test_sloppy_press_ignored(void) {
    puts("sloppy_press_ignored");

    /* Press duration between TAP_MAX_MS and hold_min_ms → nothing emitted. */
    input_init();
    input_sim_set_hold_min_ms(500);

    push_keydown(SDLK_SPACE);
    SDL_Delay(350);            /* > TAP_MAX_MS(300), < hold_min_ms(500) */
    push_keyup(SDLK_SPACE);
    ASSERT(input_poll() == INPUT_NONE, "sloppy press emits INPUT_NONE");
}

static void test_last_event_ms(void) {
    puts("last_event_ms");

    input_init();
    ASSERT(input_last_event_ms() == 0, "last_event_ms is 0 after init");

    SDL_Delay(2);  /* ensure SDL_GetTicks() > 0 */

    push_keydown(SDLK_SPACE);
    push_keyup(SDLK_SPACE);
    input_poll();
    unsigned long after_tap = input_last_event_ms();
    ASSERT(after_tap > 0, "last_event_ms > 0 after TAP");

    input_poll();  /* nothing queued */
    ASSERT(input_last_event_ms() == after_tap,
           "last_event_ms unchanged when INPUT_NONE returned");

    push_keydown(SDLK_d);
    input_poll();
    ASSERT(input_last_event_ms() >= after_tap,
           "last_event_ms non-decreasing on DOUBLE_TAP shortcut");

    push_keydown(SDLK_h);
    input_poll();
    ASSERT(input_last_event_ms() >= after_tap,
           "last_event_ms non-decreasing on HOLD shortcut");
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

    push_keydown(SDLK_q);
    input_poll();
    ASSERT(input_sim_should_quit(), "should_quit set on Q key");
}

static void test_one_event_per_frame(void) {
    puts("one_event_per_frame");

    input_init();
    push_keydown(SDLK_h);   /* HOLD */
    push_keydown(SDLK_d);   /* DOUBLE_TAP */
    ASSERT(input_poll() == INPUT_HOLD,       "first queued event returned");
    ASSERT(input_poll() == INPUT_DOUBLE_TAP, "second event returned next poll");
}

/* ------------------------------------------------------------------ */

int main(void) {
    SDL_Init(SDL_INIT_EVENTS);

    puts("=== input tests ===");
    test_tap_on_quick_press();
    test_double_tap();
    test_hold_via_space();
    test_hold_shortcuts();
    test_unknown_keys_ignored();
    test_sloppy_press_ignored();
    test_last_event_ms();
    test_quit_flag();
    test_one_event_per_frame();

    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');

    SDL_Quit();
    return tests_failed ? 1 : 0;
}
