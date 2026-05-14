#include <stdio.h>
#include <stdint.h>
#include "../display.h"
#include "../conway.h"
#include "display_buf.h"

/* Display stubs: tests exercise the framebuffer but don't need SDL2. */
void display_init(void) { display_buf_init(); }
void display_show(void) {}

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do {                                   \
    tests_run++;                                                 \
    if (cond) {                                                  \
        tests_passed++;                                          \
        printf("  PASS  %s\n", msg);                            \
    } else {                                                     \
        tests_failed++;                                          \
        printf("  FAIL  %s  (line %d)\n", msg, __LINE__);       \
    }                                                            \
} while (0)

/* Trigger exactly one generation step from a clean state.
   Must exceed GEN_MS (750 ms); last_step_ms is 0 after conway_clear_grid. */
#define ONE_STEP() conway_update(800UL)

/* ------------------------------------------------------------------ */
/* Helper: set up a clean grid with specific live cells, then step.   */

static void setup(void) {
    display_init();
    conway_clear_grid();
}

/* ------------------------------------------------------------------ */

static void test_underpopulation(void) {
    puts("underpopulation");

    /* 0 neighbors → die */
    setup();
    conway_set_cell(5, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "0 neighbours: cell dies");

    /* 1 neighbor → die */
    setup();
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "1 neighbour: left cell dies");
    ASSERT(conway_get_cell(6, 12) == 0, "1 neighbour: right cell dies");
}

static void test_survival(void) {
    puts("survival");

    /* 2×2 block: each cell has exactly 3 neighbours, block is a still life. */
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

    /* Horizontal blinker centre cell has 2 neighbours → survives. */
    setup();
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 1, "2 neighbours: centre of blinker survives");
}

static void test_overpopulation(void) {
    puts("overpopulation");

    /* Centre cell surrounded by 4 orthogonal live neighbours → 4 neighbours → dies. */
    setup();
    conway_set_cell(5, 12, 1);  /* centre */
    conway_set_cell(5, 11, 1);  /* above  */
    conway_set_cell(5, 13, 1);  /* below  */
    conway_set_cell(4, 12, 1);  /* left   */
    conway_set_cell(6, 12, 1);  /* right  */
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "4 neighbours: centre cell dies (overpopulation)");
}

static void test_birth(void) {
    puts("birth");

    /* Right-angle at (4,4),(5,4),(4,5): the empty corner (5,5) has exactly
       3 neighbours and should be born. */
    setup();
    conway_set_cell(4, 4, 1);
    conway_set_cell(5, 4, 1);
    conway_set_cell(4, 5, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 5) == 1, "dead cell with 3 neighbours is born");

    /* A dead cell with 2 neighbours must NOT be born. */
    setup();
    conway_set_cell(4, 12, 1);
    conway_set_cell(6, 12, 1);
    /* Cell at (5,12) has 2 live neighbours. */
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "dead cell with 2 neighbours is not born");

    /* A dead cell with 4 neighbours must NOT be born. */
    setup();
    conway_set_cell(5, 11, 1);
    conway_set_cell(5, 13, 1);
    conway_set_cell(4, 12, 1);
    conway_set_cell(6, 12, 1);
    /* Cell at (5,12) has 4 live neighbours. */
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "dead cell with 4 neighbours is not born");
}

static void test_blinker(void) {
    puts("blinker");

    /* Horizontal blinker → vertical → horizontal (period 2). */
    setup();
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);

    ONE_STEP();
    /* Should become vertical. */
    ASSERT(conway_get_cell(5, 11) == 1, "blinker step1: top born");
    ASSERT(conway_get_cell(5, 12) == 1, "blinker step1: centre survives");
    ASSERT(conway_get_cell(5, 13) == 1, "blinker step1: bottom born");
    ASSERT(conway_get_cell(4, 12) == 0, "blinker step1: left dies");
    ASSERT(conway_get_cell(6, 12) == 0, "blinker step1: right dies");

    /* Step 2 would return to horizontal — matching the stored initial state.
       Oscillator detection fires instead of applying the update. */
    conway_update(1600UL);  /* 800 + 750 = 1550; use 1600 for second step */
    ASSERT(conway_in_stasis(), "blinker step2: period-2 cycle detected as stasis");
}

static void test_wrapping(void) {
    puts("wrapping");

    /* Cell at (0,0) with live neighbours at (DISPLAY_W-1, 0) and (1, 0).
       With toroidal wrapping, (DISPLAY_W-1, 0) is adjacent to (0, 0).
       2 neighbours → (0,0) survives.
       Without wrapping it would have only 1 neighbour and die. */
    setup();
    conway_set_cell(0,            0, 1);
    conway_set_cell(DISPLAY_W-1,  0, 1);
    conway_set_cell(1,            0, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(0, 0) == 1,
           "wrapping: (0,0) survives with neighbour at (DISPLAY_W-1, 0)");
    ASSERT(conway_get_cell(DISPLAY_W-1, 0) == 0,
           "wrapping: far cell dies (only 1 neighbour)");

    /* Vertical wrap: cell at (5, 0) with neighbours at (5, DISPLAY_H-1) and (5, 1). */
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

    /* A 2×2 block is a still life — after one step nothing changes. */
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

    /* Single isolated cell → 0 neighbours → all cells die → all-dead is stasis. */
    setup();
    conway_set_cell(5, 12, 1);
    ONE_STEP();
    ASSERT(conway_get_cell(5, 12) == 0, "isolated cell dies");
    ASSERT(conway_in_stasis(), "all-dead grid triggers stasis flag");
}

static void test_reseed_after_stasis(void) {
    puts("reseed_after_stasis");

    /* Drive to stasis then advance time past RESEED_MS. */
    setup();
    conway_set_cell(4, 10, 1);
    conway_set_cell(5, 10, 1);
    conway_set_cell(4, 11, 1);
    conway_set_cell(5, 11, 1);

    ONE_STEP();                           /* detects stasis at t=800 */
    ASSERT(conway_in_stasis(), "stasis detected");

    conway_update(800UL + 2000UL - 1UL); /* just before timeout: still in stasis */
    ASSERT(conway_in_stasis(), "still in stasis just before RESEED_MS");

    conway_update(800UL + 2000UL);       /* exactly at timeout: re-seed */
    ASSERT(!conway_in_stasis(), "stasis cleared after RESEED_MS");
}

static void test_drawing(void) {
    puts("drawing");

    display_init();
    conway_clear_grid();
    conway_set_cell(3, 7, 1);
    conway_set_cell(4, 7, 1);
    conway_set_cell(3, 8, 1);
    conway_set_cell(4, 8, 1);  /* 2×2 block — still life */

    /* Manually call draw by going through init path:
       conway_clear_grid + set_cell doesn't draw; trigger a fresh draw
       by calling conway_update at a time that won't step yet. */
    conway_update(0UL);  /* 0 - 0 < GEN_MS → no step, just timer check */

    /* Call conway_init to ensure draw_grid() runs on the current grid.
       We re-init, so we need to re-set cells and call update differently. */

    /* Simpler: just verify that after ONE_STEP from a 2×2 block (still life),
       the display has the correct colors. */
    display_init();
    conway_clear_grid();
    conway_set_cell(3, 7, 1);
    conway_set_cell(4, 7, 1);
    conway_set_cell(3, 8, 1);
    conway_set_cell(4, 8, 1);
    ONE_STEP();  /* stasis: grid unchanged, draw NOT called on stasis detection */

    /* conway_init() does draw immediately — use that path to verify drawing. */
    /* Actually let's test through a non-stasis step. Use a blinker. */
    display_init();
    conway_clear_grid();
    conway_set_cell(4, 12, 1);
    conway_set_cell(5, 12, 1);
    conway_set_cell(6, 12, 1);
    ONE_STEP();  /* blinker rotates → draw_grid() IS called */

    uint8_t r, g, b;
    display_get(5, 11, &r, &g, &b);
    ASSERT(r != 0 || g != 0 || b != 0, "live cell drawn with non-black color");

    display_get(4, 12, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 0, "dead cell drawn as black");
}

static void test_oscillator_detected(void) {
    puts("oscillator_detected");

    /* Horizontal blinker is a period-2 oscillator.
       Step 1: rotates to vertical — not seen before, no stasis.
       Step 2: back to horizontal — matches the initial state, stasis triggered. */
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

/* ------------------------------------------------------------------ */

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
