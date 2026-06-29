// Tests for the shared app loop using a fake input clock.
#include <stdio.h>
#include <stdint.h>
#include "../display.h"
#include "../app.h"
#include "display_buf.h"

void display_init(void) { display_buf_init(); }
void display_show(void) {}

// Fake timestamp returned by input_last_event_ms().
static unsigned long fake_last_event = 0;
unsigned long input_last_event_ms(void) { return fake_last_event; }

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do {                                   \
    tests_run++;                                                 \
    if (cond) {                                                  \
        tests_passed++;                                          \
        printf("  PASS  %s\n", msg);                             \
    } else {                                                     \
        tests_failed++;                                          \
        printf("  FAIL  %s  (line %d)\n", msg, __LINE__);        \
    }                                                            \
} while (0)

// Run idle frames and return the last timestamp used.
static unsigned long idle_until(unsigned long from, unsigned long to, unsigned long step) {
    unsigned long t = from;
    for (; t <= to; t += step)
        app_step(INPUT_NONE, t);
    return t - step;
}

static void test_screensaver_without_input(void) {
    puts("screensaver_without_input");
    fake_last_event = 0;
    app_init(0);

    idle_until(0, 29000, 100);
    ASSERT(!app_in_screensaver() && !app_in_transition(),
           "no screensaver before the timeout");

    idle_until(29100, 31000, 20);
    ASSERT(app_in_transition() || app_in_screensaver(),
           "curtain starts after timeout with no input");

    idle_until(31020, 33000, 20);
    ASSERT(app_in_screensaver(), "screensaver active once curtain completes");
    ASSERT(!app_in_transition(), "curtain finished");
}

static void test_input_resets_timeout(void) {
    puts("input_resets_timeout");
    fake_last_event = 0;
    app_init(0);

    idle_until(0, 20000, 100);
    fake_last_event = 20000;
    app_step(INPUT_TAP, 20000);

    idle_until(20100, 30100, 100);
    ASSERT(!app_in_screensaver() && !app_in_transition(),
           "tap resets the idle timer");

    idle_until(30200, 50300, 50);
    ASSERT(app_in_screensaver() || app_in_transition(),
           "screensaver starts 30 s after the last input");
}

static void test_screensaver_wakes_on_input(void) {
    puts("screensaver_wakes_on_input");
    fake_last_event = 0;
    app_init(0);

    unsigned long t = idle_until(0, 32000, 20);
    ASSERT(app_in_screensaver(), "screensaver running");

    fake_last_event = t;
    app_step(INPUT_TAP, t);
    ASSERT(app_in_transition(), "tap starts the exit curtain");

    idle_until(t + 20, t + 1500, 20);
    ASSERT(!app_in_screensaver() && !app_in_transition(),
           "screensaver dismissed after the curtain");
}

static void test_hold_cycles_game(void) {
    puts("hold_cycles_game");
    fake_last_event = 0;
    app_init(100);
    ASSERT(app_num_games() >= 2, "rotation has multiple games");
    ASSERT(app_current_game() == 0, "starts on the first game");

    fake_last_event = 100;
    app_step(INPUT_HOLD, 100);
    idle_until(120, 1200, 20);
    ASSERT(app_current_game() == 1, "hold cycled to the next game");
}

int main(void) {
    test_screensaver_without_input();
    test_input_resets_timeout();
    test_screensaver_wakes_on_input();
    test_hold_cycles_game();

    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');
    return tests_failed ? 1 : 0;
}
