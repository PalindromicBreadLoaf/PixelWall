#include "stacker.h"
#include "display.h"
#include "eeprom.h"
#include <string.h>
#include <stdlib.h>

#define COLS          DISPLAY_W
#define ROWS          DISPLAY_H
#define INITIAL_WIDTH  3
#define INITIAL_SPEED  200UL   /* ms per block move at start */
#define MIN_SPEED       50UL   /* fastest the block can move */
#define SPEED_STEP       8UL   /* ms shaved off per locked row */

/* End sequence: ANIM_PHASE_MS of fall/confetti, then SCORE_PHASE_MS of score
   display, then the game reports is_over. */
#define ANIM_PHASE_MS   1000UL  /* fall animation / confetti duration */
#define SCORE_PHASE_MS  1800UL  /* score count-up display duration */
#define END_DELAY_MS    (ANIM_PHASE_MS + SCORE_PHASE_MS)

#define SCORE_STEP_MS     72UL  /* ms between successive score dots appearing */

#define N_CONFETTI       14
#define CONFETTI_STEP_MS 120UL
#define FALL_PAUSE_MS     400UL
#define FALL_STEP_MS       65UL

/* EEPROM address for Stacker best score (rows locked). */
#define STACKER_EEPROM_ADDR 1

static const uint8_t CONF_COLS[7][3] = {
    {255,  50,  50},
    {255, 160,   0},
    {220, 220,   0},
    {  0, 255,  80},
    {  0, 200, 255},
    { 80,  80, 255},
    {220,   0, 255},
};
#define N_CONF_COLS 7

static struct { uint8_t x, y, r, g, b; } confetti[N_CONFETTI];
static unsigned long last_confetti_ms;

static int           fall_offset;
static int           fall_vel;
static int           fall_done;
static unsigned long last_fall_ms;

/* Moving block color */
#define BLK_R 255
#define BLK_G 255
#define BLK_B 255

/* Locked row color */
#define LCK_R  30
#define LCK_G 100
#define LCK_B 255

static uint8_t  locked[ROWS][COLS];
static int      stack_row;       /* row currently being filled */
static int      block_x;         /* left edge of moving block */
static int      block_width;
static int      direction;       /* +1 right, -1 left */
static unsigned long speed_ms;
static unsigned long last_move_ms;
static int           in_end_state; /* 1 once the game has ended */
static unsigned long end_time_ms;
static unsigned long current_ms;
static int      locks_done;      /* rows locked so far (drives speed) */
static int      game_won;

/* ------------------------------------------------------------------ */

static void draw(void) {
    display_clear();

    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (locked[y][x])
                display_set((uint8_t)x, (uint8_t)y, LCK_R, LCK_G, LCK_B);

    if (!in_end_state)
        for (int x = block_x; x < block_x + block_width; x++)
            display_set((uint8_t)x, (uint8_t)stack_row, BLK_R, BLK_G, BLK_B);
}

static void init_confetti(void) {
    for (int i = 0; i < N_CONFETTI; i++) {
        int ci = (i * 3) % N_CONF_COLS;
        confetti[i].x = (uint8_t)(rand() % COLS);
        confetti[i].y = (uint8_t)(rand() % ROWS);
        confetti[i].r = CONF_COLS[ci][0];
        confetti[i].g = CONF_COLS[ci][1];
        confetti[i].b = CONF_COLS[ci][2];
    }
    last_confetti_ms = 0;
}

static void draw_score(void) {
    unsigned long score_elapsed = current_ms - end_time_ms - ANIM_PHASE_MS;
    int dots = (int)(score_elapsed / SCORE_STEP_MS);
    if (dots > locks_done) dots = locks_done;
    display_clear();
    for (int i = 0; i < dots; i++)
        display_set(DISPLAY_W / 2, DISPLAY_H - 1 - i, 255, 200, 50);
}

static void draw_end_frame(void) {
    unsigned long elapsed = current_ms - end_time_ms;

    /* Phase 2: score display (both win and lose). */
    if (elapsed >= ANIM_PHASE_MS) {
        draw_score();
        return;
    }

    if (!game_won) {
        /* Phase 1 (lose): brief red freeze, then blocks fall. */
        if (elapsed < FALL_PAUSE_MS) {
            display_clear();
            for (int y = 0; y < ROWS; y++)
                for (int x = 0; x < COLS; x++)
                    if (locked[y][x])
                        display_set((uint8_t)x, (uint8_t)y, 180, 0, 0);
            return;
        }
        if (!fall_done && current_ms - last_fall_ms >= FALL_STEP_MS) {
            last_fall_ms = current_ms;
            fall_vel++;
            fall_offset += fall_vel;
            if (fall_offset >= ROWS)
                fall_done = 1;
        }
        display_clear();
        if (!fall_done) {
            for (int y = 0; y < ROWS; y++) {
                for (int x = 0; x < COLS; x++) {
                    if (!locked[y][x]) continue;
                    int ny = y + fall_offset;
                    if (ny < ROWS)
                        display_set((uint8_t)x, (uint8_t)ny, LCK_R, LCK_G, LCK_B);
                }
            }
        }
        return;
    }

    /* Phase 1 (win): twinkling confetti. */
    if (current_ms - last_confetti_ms >= CONFETTI_STEP_MS) {
        last_confetti_ms = current_ms;
        for (int i = 0; i < 3; i++) {
            int idx = rand() % N_CONFETTI;
            int ci  = rand() % N_CONF_COLS;
            confetti[idx].x = (uint8_t)(rand() % COLS);
            confetti[idx].y = (uint8_t)(rand() % ROWS);
            confetti[idx].r = CONF_COLS[ci][0];
            confetti[idx].g = CONF_COLS[ci][1];
            confetti[idx].b = CONF_COLS[ci][2];
        }
    }
    display_clear();
    for (int i = 0; i < N_CONFETTI; i++)
        display_set(confetti[i].x, confetti[i].y,
                    confetti[i].r, confetti[i].g, confetti[i].b);
}

/* ------------------------------------------------------------------ */

void stacker_init(void) {
    memset(locked, 0, sizeof(locked));
    stack_row    = ROWS - 1;
    block_x      = 0;
    block_width  = INITIAL_WIDTH;
    direction    = 1;
    speed_ms     = INITIAL_SPEED;
    last_move_ms     = 0;
    in_end_state     = 0;
    end_time_ms      = 0;
    current_ms       = 0;
    locks_done       = 0;
    game_won         = 0;
    last_confetti_ms = 0;
    fall_offset      = 0;
    fall_vel         = 0;
    fall_done        = 0;
    last_fall_ms     = 0;
    draw();
}

void stacker_update(unsigned long now_ms) {
    current_ms = now_ms;

    if (in_end_state) {
        draw_end_frame();
        return;
    }

    if (now_ms - last_move_ms < speed_ms) return;
    last_move_ms = now_ms;

    block_x += direction;
    if (block_x + block_width > COLS) {
        int over  = block_x + block_width - COLS;
        block_x   = COLS - block_width - over;
        direction = -1;
    } else if (block_x < 0) {
        int over  = -block_x;
        block_x   = over;
        direction = 1;
    }

    draw();
}

void stacker_on_input(InputEvent ev) {
    if (ev != INPUT_TAP || in_end_state) return;

    int any_locked = 0;

    if (stack_row == ROWS - 1) {
        /* First row: no row below, lock whatever the block covers. */
        for (int x = block_x; x < block_x + block_width; x++) {
            locked[stack_row][x] = 1;
            any_locked = 1;
        }
    } else {
        /* Subsequent rows: only keep pixels that overlap the row below. */
        for (int x = block_x; x < block_x + block_width; x++) {
            if (locked[stack_row + 1][x]) {
                locked[stack_row][x] = 1;
                any_locked = 1;
            }
        }
    }

    if (!any_locked) {
        /* Save score if it beats the stored high score. */
        uint8_t hi = eeprom_read(STACKER_EEPROM_ADDR);
        if (hi == 0xFF || locks_done > hi)
            eeprom_write(STACKER_EEPROM_ADDR, (uint8_t)locks_done);
        in_end_state = 1;
        end_time_ms  = current_ms;
        game_won     = 0;
        fall_offset  = 0;
        fall_vel     = 0;
        fall_done    = 0;
        last_fall_ms = current_ms;
        return;
    }

    locks_done++;

    if (stack_row == 0) {
        /* Save score if it beats the stored high score. */
        uint8_t hi = eeprom_read(STACKER_EEPROM_ADDR);
        if (hi == 0xFF || locks_done > hi)
            eeprom_write(STACKER_EEPROM_ADDR, (uint8_t)locks_done);
        in_end_state = 1;
        end_time_ms  = current_ms;
        game_won     = 1;
        init_confetti();
        return;
    }

    /* Find the extent of the newly locked pixels (always contiguous). */
    int lx_start = COLS, lx_end = -1;
    for (int x = 0; x < COLS; x++) {
        if (locked[stack_row][x]) {
            if (x < lx_start) lx_start = x;
            if (x > lx_end)   lx_end   = x;
        }
    }
    block_width = lx_end - lx_start + 1;
    block_x     = lx_start;

    /* Increase speed. */
    unsigned long new_speed = INITIAL_SPEED - (unsigned long)locks_done * SPEED_STEP;
    speed_ms = (new_speed > MIN_SPEED) ? new_speed : MIN_SPEED;

    stack_row--;
    last_move_ms = current_ms;

    draw();
}

int stacker_is_over(void) {
    return in_end_state && (current_ms - end_time_ms >= END_DELAY_MS);
}

/* ------------------------------------------------------------------ */

int stacker_get_stack_row(void)        { return stack_row;  }
int stacker_get_block_x(void)          { return block_x;    }
int stacker_get_block_width(void)      { return block_width; }
unsigned long stacker_get_speed_ms(void) { return speed_ms; }
int stacker_won(void)                  { return game_won;   }

int stacker_is_cell_locked(uint8_t x, uint8_t y) {
    if (x >= COLS || y >= ROWS) return 0;
    return locked[y][x];
}

void stacker_set_block_x(int x) {
    block_x = x;
}

/* ------------------------------------------------------------------ */

const Game stacker_game = {
    "Stacker",
    stacker_init,
    stacker_update,
    stacker_on_input,
    stacker_is_over
};
