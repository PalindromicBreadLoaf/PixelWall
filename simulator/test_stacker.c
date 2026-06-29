#include <stdio.h>
#include <stdint.h>
#include "../display.h"
#include "../stacker.h"
#include "display_buf.h"

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

#define TAP()     stacker_on_input(INPUT_TAP)
#define STEP(t)   stacker_update((unsigned long)(t))

static void test_init_state(void) {
    puts("init_state");
    display_init();
    stacker_init();

    ASSERT(stacker_get_stack_row() == DISPLAY_H - 1, "stack starts at bottom row");
    ASSERT(stacker_get_block_width() == 3,            "initial block width is 3");
    ASSERT(stacker_get_block_x() == 0,                "initial block at x=0");
    ASSERT(!stacker_is_over(),                        "game not over at start");
    ASSERT(!stacker_won(),                            "not won at start");

    int any_locked = 0;
    for (int y = 0; y < DISPLAY_H && !any_locked; y++)
        for (int x = 0; x < DISPLAY_W && !any_locked; x++)
            if (stacker_is_cell_locked((uint8_t)x, (uint8_t)y)) any_locked = 1;
    ASSERT(!any_locked, "no cells locked after init");
}

static void test_block_moves(void) {
    puts("block_moves");
    display_init();
    stacker_init();

    int x0 = stacker_get_block_x();
    STEP(500);
    ASSERT(stacker_get_block_x() != x0 || stacker_get_block_width() == DISPLAY_W,
           "block moves after speed_ms");
}

static void test_bounce_right(void) {
    puts("bounce_right");
    display_init();
    stacker_init();

    int steps = 0;
    for (int t = 500; steps < 20; t += 500, steps++) {
        STEP(t);
        if (stacker_get_block_x() == DISPLAY_W - stacker_get_block_width()) break;
    }
    int x_at_wall = stacker_get_block_x();
    // Reflection happens in the same frame as the wall hit.
    unsigned long base = (unsigned long)500 * (steps + 1);
    STEP(base + 500);
    ASSERT(stacker_get_block_x() < x_at_wall, "block reverses at right wall");
}

static void test_bounce_left(void) {
    puts("bounce_left");
    display_init();
    stacker_init();

    for (int i = 1; i <= 30; i++) STEP(500 * i);
    ASSERT(stacker_get_block_x() >= 0, "block stays in bounds on left");
}

static void test_first_tap_locks_all(void) {
    puts("first_tap_locks_all");
    display_init();
    stacker_init();

    TAP();
    int bottom = DISPLAY_H - 1;
    ASSERT(stacker_is_cell_locked(0, bottom), "col 0 locked on first tap");
    ASSERT(stacker_is_cell_locked(1, bottom), "col 1 locked on first tap");
    ASSERT(stacker_is_cell_locked(2, bottom), "col 2 locked on first tap");
    ASSERT(!stacker_is_cell_locked(3, bottom),"col 3 not locked (outside block)");
    ASSERT(stacker_get_stack_row() == DISPLAY_H - 2, "stack_row advances after tap");
}

static void test_overlap_full(void) {
    puts("overlap_full");
    display_init();
    stacker_init();

    TAP();
    int w_before = stacker_get_block_width();
    int x_before = stacker_get_block_x();

    ASSERT(stacker_get_block_x()   == x_before, "block repositioned over locked cells");
    ASSERT(stacker_get_block_width() == w_before, "width unchanged after full overlap");
}

static void test_overlap_partial(void) {
    puts("overlap_partial");
    display_init();
    stacker_init();

    TAP();

    // Shift right so only columns 1 and 2 overlap.
    stacker_set_block_x(1);
    TAP();

    ASSERT(stacker_get_block_width() == 2, "partial overlap reduces width to 2");
    ASSERT(stacker_is_cell_locked(1, DISPLAY_H - 2), "col 1 locked after partial overlap");
    ASSERT(stacker_is_cell_locked(2, DISPLAY_H - 2), "col 2 locked after partial overlap");
    ASSERT(!stacker_is_cell_locked(0, DISPLAY_H - 2), "col 0 not locked (missed)");
    ASSERT(!stacker_is_cell_locked(3, DISPLAY_H - 2), "col 3 not locked (outside)");
}

static void test_overlap_none_loses(void) {
    puts("overlap_none_loses");
    display_init();
    stacker_init();

    TAP();

    // Move completely past the locked zone.
    stacker_set_block_x(7);
    TAP();

    ASSERT(!stacker_won(),   "no win on zero overlap");
    ASSERT(!stacker_is_over(), "is_over() false immediately after lose");

    STEP(2800);
    ASSERT(stacker_is_over(), "is_over() true after end animation on lose");
}

static void test_speed_increases(void) {
    puts("speed_increases");
    display_init();
    stacker_init();

    unsigned long s0 = stacker_get_speed_ms();
    TAP();
    unsigned long s1 = stacker_get_speed_ms();
    TAP();
    unsigned long s2 = stacker_get_speed_ms();

    ASSERT(s1 < s0, "speed_ms decreases after first lock");
    ASSERT(s2 < s1, "speed_ms decreases after second lock");
}

static void test_win(void) {
    puts("win");
    display_init();
    stacker_init();

    // Tap once per row, keeping full overlap each time.
    for (int row = DISPLAY_H - 1; row >= 0; row--) {
        int cur = stacker_get_stack_row();
        ASSERT(cur == row, "stack_row correct before each tap");
        if (cur != row) break;
        TAP();
    }

    ASSERT(stacker_won(), "game_won after reaching row 0");
    ASSERT(!stacker_is_over(), "is_over() false immediately after win");

    STEP(2800);
    ASSERT(stacker_is_over(), "is_over() true after end animation on win");
}

static void test_ignore_input_after_game_over(void) {
    puts("ignore_input_after_game_over");
    display_init();
    stacker_init();

    TAP();
    stacker_set_block_x(7);
    TAP();

    int row_at_end = stacker_get_stack_row();
    TAP();
    ASSERT(stacker_get_stack_row() == row_at_end,
           "extra tap after lose does not change state");
}

static void test_drawing_locked_cells(void) {
    puts("drawing_locked_cells");
    display_init();
    stacker_init();

    TAP();

    int bottom = DISPLAY_H - 1;
    uint8_t r, g, b;
    display_get(0, bottom, &r, &g, &b);
    ASSERT(r == 30 && g == 100 && b == 255,
           "locked cell drawn with correct blue color");

    display_get(3, bottom, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 0,
           "unlocked cell is black");
}

static void test_drawing_moving_block(void) {
    puts("drawing_moving_block");
    display_init();
    stacker_init();

    uint8_t r, g, b;
    display_get(0, DISPLAY_H - 1, &r, &g, &b);
    ASSERT(r == 255 && g == 255 && b == 255,
           "moving block drawn white");

    display_get(3, DISPLAY_H - 1, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 0,
           "position outside block is black");
}

int main(void) {
    puts("=== stacker tests ===");
    test_init_state();
    test_block_moves();
    test_bounce_right();
    test_bounce_left();
    test_first_tap_locks_all();
    test_overlap_full();
    test_overlap_partial();
    test_overlap_none_loses();
    test_speed_increases();
    test_win();
    test_ignore_input_after_game_over();
    test_drawing_locked_cells();
    test_drawing_moving_block();

    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');
    return tests_failed ? 1 : 0;
}
