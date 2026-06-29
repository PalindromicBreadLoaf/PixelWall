#include <stdio.h>
#include <stdint.h>
#include "../display.h"
#include "../conway.h"
#include "display_buf.h"

// Tests use the raw framebuffer instead of SDL.
void display_init(void) { display_buf_init(); }
void display_show(void) {}

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

// One generation from a freshly cleared grid.
#define ONE_STEP() conway_update(800UL)
#define FINISH_STEP_FADE() conway_update(1100UL)

static void setup(void) {
    display_init();
    conway_clear_grid();
}

static void test_underpopulation(void) {
    puts("underpopulation");

    setup();
    conway_set_cell(5, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "0 neighbours: cell dies");

    setup();
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "1 neighbour: left cell dies");
    ASSERT(conway_get_cell(6, 12) == 0, "1 neighbour: right cell dies");
}

static void test_survival(void) {
    puts("survival");

    // A 2x2 block is a still life.
    setup();
    conway_set_cell(4, 10, 1);
    conway_set_cell(5, 10, 1);
    conway_set_cell(4, 11, 1);
    conway_set_cell(5, 11, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(4, 10) == 1, "2x2 block survives (top-left)");
    ASSERT(conway_get_cell(5, 10) == 1, "2x2 block survives (top-right)");
    ASSERT(conway_get_cell(4, 11) == 1, "2x2 block survives (bottom-left)");
    ASSERT(conway_get_cell(5, 11) == 1, "2x2 block survives (bottom-right)");

    setup();
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 1, "2 neighbours: centre of blinker survives");
}

static void test_overpopulation(void) {
    puts("overpopulation");

    setup();
    conway_set_cell(5, 12, 1);
    conway_set_cell(5, 11, 1);
    conway_set_cell(5, 13, 1);
    conway_set_cell(4, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "4 neighbours: centre cell dies (overpopulation)");
}

static void test_birth(void) {
    puts("birth");

    // The empty corner has three live neighbours.
    setup();
    conway_set_cell(4, 4, 1);
    conway_set_cell(5, 4, 1);
    conway_set_cell(4, 5, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 5) == 1, "dead cell with 3 neighbours is born");

    setup();
    conway_set_cell(4, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "dead cell with 2 neighbours is not born");

    setup();
    conway_set_cell(5, 11, 1);
    conway_set_cell(5, 13, 1);
    conway_set_cell(4, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "dead cell with 4 neighbours is not born");
}

static void test_blinker(void) {
    puts("blinker");

    // Horizontal blinker becomes vertical after one step.
    setup();
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);

    ONE_STEP();
    ASSERT(conway_get_cell(5, 11) == 1, "blinker step1: top born");
    ASSERT(conway_get_cell(5, 12) == 1, "blinker step1: centre survives");
    ASSERT(conway_get_cell(5, 13) == 1, "blinker step1: bottom born");
    ASSERT(conway_get_cell(4, 12) == 0, "blinker step1: left dies");
    ASSERT(conway_get_cell(6, 12) == 0, "blinker step1: right dies");

    // Step 2 would repeat the starting grid, so stasis is detected.
    conway_update(1600UL);
    ASSERT(conway_in_stasis(), "blinker step2: period-2 cycle detected as stasis");
}

static void test_wrapping(void) {
    puts("wrapping");

    // The left and right edges are neighbours.
    setup();
    conway_set_cell(0,            0, 1);
    conway_set_cell(DISPLAY_W-1,  0, 1);
    conway_set_cell(1,            0, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(0, 0) == 1,
           "wrapping: (0,0) survives with neighbour at (DISPLAY_W-1, 0)");
    ASSERT(conway_get_cell(DISPLAY_W-1, 0) == 0,
           "wrapping: far cell dies (only 1 neighbour)");

    // The top and bottom edges are neighbours too.
    setup();
    conway_set_cell(5, 0,            1);
    conway_set_cell(5, DISPLAY_H-1,  1);
    conway_set_cell(5, 1,            1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 0) == 1,
           "wrapping: (5,0) survives with neighbour at (5, DISPLAY_H-1)");
}

static void test_still_life_no_stasis_flag(void) {
    puts("stasis_detection");

    setup();
    conway_set_cell(4, 10, 1);
    conway_set_cell(5, 10, 1);
    conway_set_cell(4, 11, 1);
    conway_set_cell(5, 11, 1);

    ASSERT(!conway_in_stasis(), "no stasis before any step");
    ONE_STEP();
    ASSERT(conway_in_stasis(), "stasis detected after still life step");
}

static void test_extinction_triggers_stasis(void) {
    puts("extinction_triggers_stasis");

    setup();
    conway_set_cell(5, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "isolated cell dies");
    ASSERT(conway_in_stasis(), "all-dead grid triggers stasis flag");
}

static void test_reseed_after_stasis(void) {
    puts("reseed_after_stasis");

    setup();
    conway_set_cell(4, 10, 1);
    conway_set_cell(5, 10, 1);
    conway_set_cell(4, 11, 1);
    conway_set_cell(5, 11, 1);

    ONE_STEP();
    ASSERT(conway_in_stasis(), "stasis detected");

    conway_update(800UL + 2000UL - 1UL);
    ASSERT(conway_in_stasis(), "still in stasis just before RESEED_MS");

    conway_update(800UL + 2000UL);
    ASSERT(!conway_in_stasis(), "stasis cleared after RESEED_MS");
}

static void test_drawing(void) {
    puts("drawing");

    // A blinker step changes the grid and redraws the display.
    display_init();
    conway_clear_grid();
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    FINISH_STEP_FADE();

    uint8_t r, g, b;
    display_get(5, 11, &r, &g, &b);
    int live_sum = (int)r + g + b;
    ASSERT(live_sum != 0, "live cell drawn with non-black color");

    display_get(4, 12, &r, &g, &b);
    int dead_sum = (int)r + g + b;
    ASSERT(dead_sum != 0, "non-alive cell drawn with non-black dim color");
    ASSERT(dead_sum < live_sum, "non-alive cell dimmer overall than live cell");
}

static void test_oscillator_detected(void) {
    puts("oscillator_detected");

    // A period-2 blinker should trigger stasis when it repeats.
    setup();
    conway_set_cell(3, 12, 1);
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);

    ASSERT(!conway_in_stasis(), "not in stasis before any step");

    conway_update(800UL);
    ASSERT(!conway_in_stasis(), "not in stasis after step 1 (new state)");

    conway_update(1600UL);
    ASSERT(conway_in_stasis(), "period-2 oscillator detected after returning to initial state");
}

static void test_on_input_is_noop(void) {
    puts("on_input_noop");

    setup();
    conway_set_cell(5, 12, 1);
    conway_on_input(INPUT_TAP);
    conway_on_input(INPUT_HOLD);
    conway_on_input(INPUT_DOUBLE_TAP);
    ASSERT(conway_get_cell(5, 12) == 1, "on_input does not modify grid");
}

static void test_is_over_always_false(void) {
    puts("is_over_always_false");

    conway_init();
    ASSERT(conway_is_over() == 0, "is_over returns 0 after init");
    ONE_STEP();
    ASSERT(conway_is_over() == 0, "is_over returns 0 after step");
}

int main(void) {
    puts("=== conway tests ===");
    test_underpopulation();
    test_survival();
    test_overpopulation();
    test_birth();
    test_blinker();
    test_wrapping();
    test_still_life_no_stasis_flag();
    test_extinction_triggers_stasis();
    test_reseed_after_stasis();
    test_drawing();
    test_oscillator_detected();
    test_on_input_is_noop();
    test_is_over_always_false();

    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');
    return tests_failed ? 1 : 0;
}
