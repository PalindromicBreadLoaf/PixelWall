#include "stacker.h"
#include "display.h"
#include <string.h>

#define COLS          DISPLAY_W
#define ROWS          DISPLAY_H
#define INITIAL_WIDTH  3
#define INITIAL_SPEED  200UL   /* ms per block move at start */
#define MIN_SPEED       50UL   /* fastest the block can move */
#define SPEED_STEP       8UL   /* ms shaved off per locked row */
#define END_DELAY_MS  1500UL   /* time to show win/lose animation */
#define BLINK_MS       200UL   /* animation blink interval */

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

static void draw_end_frame(void) {
    unsigned long elapsed = current_ms - end_time_ms;
    int on = (int)((elapsed / BLINK_MS) % 2);

    display_clear();
    if (on) {
        uint8_t r = game_won ? 255 : 255;
        uint8_t g = game_won ? 255 :   0;
        uint8_t b = game_won ? 255 :   0;
        for (int y = 0; y < ROWS; y++)
            for (int x = 0; x < COLS; x++)
                display_set((uint8_t)x, (uint8_t)y, r, g, b);
    }
}

/* ------------------------------------------------------------------ */

void stacker_init(void) {
    memset(locked, 0, sizeof(locked));
    stack_row    = ROWS - 1;
    block_x      = 0;
    block_width  = INITIAL_WIDTH;
    direction    = 1;
    speed_ms     = INITIAL_SPEED;
    last_move_ms = 0;
    in_end_state = 0;
    end_time_ms  = 0;
    current_ms   = 0;
    locks_done   = 0;
    game_won     = 0;
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
        in_end_state = 1;
        end_time_ms  = current_ms;
        game_won     = 0;
        return;
    }

    locks_done++;

    if (stack_row == 0) {
        in_end_state = 1;
        end_time_ms  = current_ms;
        game_won     = 1;
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
