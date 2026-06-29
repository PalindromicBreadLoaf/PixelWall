#include <stdio.h>
#include <stdint.h>
#include "../display.h"
#include "../space_invaders.h"
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

#define STEP(ms)  si_update((unsigned long)(ms))
#define TAP()     si_on_input(INPUT_TAP)

// Test copy of the invader grid layout.
#define INV_COLS       4
#define INV_ROWS       3
#define INV_X_SPACING  2
#define INV_Y_SPACING  2
#define PLAYER_ROW    (DISPLAY_H - 1)

static void test_init_state(void) {
    puts("init_state");
    display_init();
    si_init();

    ASSERT(si_get_level()       == 1,          "level is 1 after init");
    ASSERT(si_get_player_x()    == 0,          "player starts at x=0");
    ASSERT(si_get_bullet_y()    == -1,         "no player bullet at start");
    ASSERT(si_get_inv_bullet_y() == -1,        "no invader bullet at start");
    ASSERT(si_get_alive_count() == INV_ROWS * INV_COLS, "all invaders alive");
    ASSERT(!si_in_end_state(),                 "not in end state");

    int all = 1;
    for (int r = 0; r < INV_ROWS && all; r++)
        for (int c = 0; c < INV_COLS && all; c++)
            if (!si_get_invader_alive(r, c)) all = 0;
    ASSERT(all, "all individual invader cells alive");
}

static void test_player_oscillates(void) {
    puts("player_oscillates");
    display_init();
    si_init();

    int x0 = si_get_player_x();
    STEP(200);
    ASSERT(si_get_player_x() != x0, "player moves after PLAYER_SPEED_MS");

    for (int t = 400; t <= 4000; t += 200) STEP(t);
    ASSERT(si_get_player_x() <= DISPLAY_W - 1, "player stays in bounds (right)");

    for (int t = 4200; t <= 8000; t += 200) STEP(t);
    ASSERT(si_get_player_x() >= 0, "player stays in bounds (left)");
}

static void test_tap_fires_bullet(void) {
    puts("tap_fires_bullet");
    display_init();
    si_init();

    ASSERT(si_get_bullet_y() == -1, "no bullet before tap");

    STEP(0);
    TAP();
    ASSERT(si_get_bullet_y() == PLAYER_ROW - 1, "bullet appears one row above player");
    ASSERT(si_get_bullet_x() == si_get_player_x(), "bullet x matches player x");
}

static void test_bullet_moves_up(void) {
    puts("bullet_moves_up");
    display_init();
    si_init();

    STEP(0);
    TAP();
    int y0 = si_get_bullet_y();

    STEP(60);
    ASSERT(si_get_bullet_y() == y0 - 1, "bullet moves up one row per tick");
}

static void test_only_one_bullet(void) {
    puts("only_one_bullet");
    display_init();
    si_init();

    STEP(0); TAP();
    int y0 = si_get_bullet_y();
    TAP();
    ASSERT(si_get_bullet_y() == y0, "second tap ignored while bullet in flight");
}

static void test_bullet_hits_invader(void) {
    puts("bullet_hits_invader");
    display_init();
    si_init();

    // Start one row below the top-left invader.
    si_set_bullet(0, 2);
    STEP(60);

    ASSERT(!si_get_invader_alive(0, 0), "invader [0][0] killed");
    ASSERT(si_get_bullet_y() == -1,     "bullet cleared after hit");
    ASSERT(si_get_alive_count() == INV_ROWS * INV_COLS - 1, "total alive count decremented");
}

static void test_bullet_clears_at_top(void) {
    puts("bullet_clears_at_top");
    display_init();
    si_init();

    si_set_bullet(5, 1);
    STEP(60);
    ASSERT(si_get_bullet_y() == 0 || si_get_bullet_y() == -1,
           "bullet at row 0 or cleared");

    STEP(120);
    ASSERT(si_get_bullet_y() == -1, "bullet cleared after leaving display");
}

static void test_invaders_move(void) {
    puts("invaders_move");
    display_init();
    si_init();

    int x0 = si_get_group_x();
    STEP(1000);
    ASSERT(si_get_group_x() != x0, "invader group moves after inv_move_ms");
}

static void test_invaders_bounce_and_descend(void) {
    puts("invaders_bounce_and_descend");
    display_init();
    si_init();

    // Step long enough to hit the wall and descend.
    unsigned long t = 1000;
    for (int i = 0; i < 20; i++, t += 1000) {
        int prev_x = si_get_group_x();
        STEP(t);
        if (si_get_group_x() <= prev_x) {
            break;
        }
    }

    int y0 = si_get_group_y();
    ASSERT(y0 > 1, "group descended after hitting a wall");
}

static void test_game_over_invader_reaches_player(void) {
    puts("game_over_invader_reaches_player");
    display_init();
    si_init();

    // Put the bottom row just above the game-over threshold.
    si_set_group_y(19);

    STEP(1000);
    ASSERT(si_in_end_state(), "game over when invader reaches player row");
}

static void test_level_advances_on_all_dead(void) {
    puts("level_advances_on_all_dead");
    display_init();
    si_init();

    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            si_kill_invader(r, c);

    ASSERT(si_get_alive_count() == 0, "all invaders dead after kill_all");

    // Level win only triggers from the bullet-hit path.
    si_init();
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (!(r == 0 && c == 0))
                si_kill_invader(r, c);

    si_set_bullet(0, 2);
    STEP(60);

    ASSERT(si_in_end_state(), "end state triggered when last invader killed");
    ASSERT(si_get_level() == 1, "level still 1 during transition animation");

    STEP(60 + 1200);
    ASSERT(si_get_level() == 2, "level advances to 2 after delay");
    ASSERT(si_get_alive_count() == INV_ROWS * INV_COLS, "invaders reset for new level");
    ASSERT(!si_in_end_state(), "end state cleared at new level");
}

static void test_game_win_after_max_level(void) {
    puts("game_win_after_max_level");
    display_init();
    si_init();

    // Reach level 4 by winning levels 1 through 3.
    for (int lv = 1; lv < 4; lv++) {
        for (int r = 0; r < INV_ROWS; r++)
            for (int c = 0; c < INV_COLS; c++)
                if (!(r == 0 && c == 0))
                    si_kill_invader(r, c);
        si_set_bullet(0, 2);
        STEP((unsigned long)(lv * 100000 + 60));
        STEP((unsigned long)(lv * 100000 + 60 + 1200));
    }

    ASSERT(si_get_level() == 4, "reached level 4");

    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (!(r == 0 && c == 0))
                si_kill_invader(r, c);
    si_set_bullet(0, 2);
    STEP(500000 + 60);

    ASSERT(si_in_end_state(), "end state on last invader at level 4");
    ASSERT(!si_is_over(), "is_over false before END_DELAY_MS");
    STEP(500000 + 60 + 1500);
    ASSERT(si_is_over(), "is_over true after END_DELAY_MS on game win");
}

static void test_invader_bullet_hits_player(void) {
    puts("invader_bullet_hits_player");
    display_init();
    si_init();

    int px = si_get_player_x();
    si_set_inv_bullet(px, PLAYER_ROW - 1);

    STEP(160);
    ASSERT(si_in_end_state(), "game over when invader bullet hits player");
}

static void test_invader_bullet_misses(void) {
    puts("invader_bullet_misses");
    display_init();
    si_init();

    si_set_inv_bullet(9, PLAYER_ROW - 1);
    STEP(160);
    ASSERT(!si_in_end_state(), "no game over when invader bullet misses player");
    ASSERT(si_get_inv_bullet_y() == -1 || si_get_inv_bullet_y() == PLAYER_ROW,
           "invader bullet at player row or cleared");
}

static void test_is_over_timing(void) {
    puts("is_over_timing");
    display_init();
    si_init();

    int px = si_get_player_x();
    si_set_inv_bullet(px, PLAYER_ROW - 1);
    STEP(160);

    ASSERT(!si_is_over(), "is_over false immediately after game over");
    STEP(160 + 1499);
    ASSERT(!si_is_over(), "is_over false just before END_DELAY_MS");
    STEP(160 + 1500);
    ASSERT(si_is_over(),  "is_over true after END_DELAY_MS");
}

static void test_player_freeze_toggle(void) {
    puts("player_freeze_toggle");
    display_init();
    si_init();

    ASSERT(!si_get_player_frozen(), "player not frozen after init");

    si_on_input(INPUT_DOUBLE_TAP);
    ASSERT(si_get_player_frozen(), "double-tap freezes player");

    si_on_input(INPUT_DOUBLE_TAP);
    ASSERT(!si_get_player_frozen(), "second double-tap unfreezes player");
}

static void test_player_stays_still_when_frozen(void) {
    puts("player_stays_still_when_frozen");
    display_init();
    si_init();

    si_on_input(INPUT_DOUBLE_TAP);
    int x0 = si_get_player_x();

    STEP(1000);
    ASSERT(si_get_player_x() == x0, "frozen player does not move");

    si_on_input(INPUT_DOUBLE_TAP);
    STEP(1200);
    ASSERT(si_get_player_x() != x0, "unfrozen player resumes moving");
}

static void test_freeze_resets_on_level_change(void) {
    puts("freeze_resets_on_level_change");
    display_init();
    si_init();

    si_on_input(INPUT_DOUBLE_TAP);
    ASSERT(si_get_player_frozen(), "frozen before level transition");

    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (!(r == 0 && c == 0))
                si_kill_invader(r, c);
    si_set_bullet(0, 2);
    STEP(60);
    STEP(60 + 1200);

    ASSERT(!si_get_player_frozen(), "freeze cleared when new level starts");
}

static void test_drawing(void) {
    puts("drawing");
    display_init();
    si_init();

    uint8_t r, g, b;
    display_get((uint8_t)si_get_player_x(), PLAYER_ROW, &r, &g, &b);
    ASSERT(r == 255 && g == 255 && b == 255, "player drawn white");

    display_get(0, 1, &r, &g, &b);
    ASSERT(r == 200 && g == 0 && b == 200, "top invader drawn magenta");

    display_get(0, 3, &r, &g, &b);
    ASSERT(r == 0 && g == 200 && b == 200, "middle invader drawn cyan");

    display_get(0, 5, &r, &g, &b);
    ASSERT(r == 200 && g == 200 && b == 0, "bottom invader drawn yellow");
}

int main(void) {
    puts("=== space invaders tests ===");
    test_init_state();
    test_player_oscillates();
    test_tap_fires_bullet();
    test_bullet_moves_up();
    test_only_one_bullet();
    test_bullet_hits_invader();
    test_bullet_clears_at_top();
    test_invaders_move();
    test_invaders_bounce_and_descend();
    test_game_over_invader_reaches_player();
    test_level_advances_on_all_dead();
    test_game_win_after_max_level();
    test_invader_bullet_hits_player();
    test_invader_bullet_misses();
    test_is_over_timing();
    test_player_freeze_toggle();
    test_player_stays_still_when_frozen();
    test_freeze_resets_on_level_change();
    test_drawing();

    printf("\n%d/%d passed", tests_passed, tests_run);
    if (tests_failed) printf("  (%d FAILED)", tests_failed);
    putchar('\n');
    return tests_failed ? 1 : 0;
}
