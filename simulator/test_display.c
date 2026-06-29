#include <stdio.h>
#include <stdint.h>
#include "../display.h"
#include "display_buf.h"

// Tests use the raw framebuffer instead of SDL.
void display_init(void) { display_buf_init(); }
void display_show(void) {}

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do {                                      \
    tests_run++;                                                    \
    if (cond) {                                                     \
        tests_passed++;                                             \
        printf("  PASS  %s\n", msg);                                \
    } else {                                                        \
        tests_failed++;                                             \
        printf("  FAIL  %s  (line %d)\n", msg, __LINE__);           \
    }                                                               \
} while (0)

static void test_set_and_get(void) {
    puts("set_and_get");
    display_init();

    uint8_t r, g, b;

    display_set(0, 0, 255, 0, 0);
    display_get(0, 0, &r, &g, &b);
    ASSERT(r == 255 && g == 0 && b == 0, "red at top-left (0,0)");

    display_set(9, 24, 0, 255, 0);
    display_get(9, 24, &r, &g, &b);
    ASSERT(r == 0 && g == 255 && b == 0, "green at bottom-right (9,24)");

    display_set(5, 12, 0, 0, 255);
    display_get(5, 12, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 255, "blue at center (5,12)");

    display_set(0, 0, 1, 2, 3);
    display_get(0, 0, &r, &g, &b);
    ASSERT(r == 1 && g == 2 && b == 3, "overwrite pixel keeps last value");
}

static void test_clear(void) {
    puts("clear");
    display_init();

    for (int y = 0; y < DISPLAY_H; y++)
        for (int x = 0; x < DISPLAY_W; x++)
            display_set((uint8_t)x, (uint8_t)y, 255, 255, 255);

    display_clear();

    uint8_t r, g, b;
    int all_zero = 1;
    for (int y = 0; y < DISPLAY_H && all_zero; y++)
        for (int x = 0; x < DISPLAY_W && all_zero; x++) {
            display_get((uint8_t)x, (uint8_t)y, &r, &g, &b);
            if (r || g || b) all_zero = 0;
        }
    ASSERT(all_zero, "all pixels zero after clear");
}

static void test_out_of_bounds(void) {
    puts("out_of_bounds");
    display_init();

    display_set(DISPLAY_W,     0,          255, 0, 0);
    display_set(0,             DISPLAY_H,  0, 255, 0);
    display_set(255,           255,        0, 0, 255);
    ASSERT(1, "out-of-bounds writes do not crash");

    uint8_t r = 99, g = 99, b = 99;
    display_get(DISPLAY_W, 0, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 0, "out-of-bounds get x=DISPLAY_W returns zero");

    r = 99; g = 99; b = 99;
    display_get(0, DISPLAY_H, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 0, "out-of-bounds get y=DISPLAY_H returns zero");
}

static void test_init_clears(void) {
    puts("init_clears");

    display_set(3, 3, 100, 100, 100);
    display_init();

    uint8_t r, g, b;
    display_get(3, 3, &r, &g, &b);
    ASSERT(r == 0 && g == 0 && b == 0, "init clears framebuffer");
}

static void test_all_pixels_independent(void) {
    puts("all_pixels_independent");
    display_init();

    for (int y = 0; y < DISPLAY_H; y++)
        for (int x = 0; x < DISPLAY_W; x++)
            display_set((uint8_t)x, (uint8_t)y,
                        (uint8_t)x, (uint8_t)y, (uint8_t)(x + y));

    int ok = 1;
    uint8_t r, g, b;
    for (int y = 0; y < DISPLAY_H && ok; y++)
        for (int x = 0; x < DISPLAY_W && ok; x++) {
            display_get((uint8_t)x, (uint8_t)y, &r, &g, &b);
            if (r != (uint8_t)x || g != (uint8_t)y || b != (uint8_t)(x + y))
                ok = 0;
        }
    ASSERT(ok, "every pixel holds its own value independently");
}

int main(void) {
    puts("=== display tests ===");
    test_set_and_get();
    test_clear();
    test_out_of_bounds();
    test_init_clears();
    test_all_pixels_independent();
    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');
    return tests_failed ? 1 : 0;
}
