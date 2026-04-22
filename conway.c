#include "conway.h"
#include "display.h"
#include <stdlib.h>
#include <string.h>

#define COLS      DISPLAY_W
#define ROWS      DISPLAY_H
#define GEN_MS    200UL   /* ms between generations */
#define RESEED_MS 2000UL  /* pause before re-seeding after stasis */

#define CELL_R   0
#define CELL_G 200
#define CELL_B  50

static uint8_t grid[ROWS][COLS];
static uint8_t next_grid[ROWS][COLS];

static unsigned long last_step_ms = 0;
static unsigned long stasis_ms    = 0;  /* 0 = not in stasis */

/* ------------------------------------------------------------------ */

static void do_seed(void) {
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            grid[y][x] = (rand() % 3 == 0) ? 1 : 0;  /* ~33% density */
    stasis_ms = 0;
}

static void draw_grid(void) {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (grid[y][x])
                display_set((uint8_t)x, (uint8_t)y, CELL_R, CELL_G, CELL_B);
            else
                display_set((uint8_t)x, (uint8_t)y, 0, 0, 0);
        }
    }
}

static int count_neighbors(int x, int y) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            n += grid[(y + dy + ROWS) % ROWS][(x + dx + COLS) % COLS];
        }
    return n;
}

static int grid_has_live(uint8_t g[ROWS][COLS]) {
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (g[y][x]) return 1;
    return 0;
}

static void compute_next(void) {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int n = count_neighbors(x, y);
            if (grid[y][x])
                next_grid[y][x] = (n == 2 || n == 3) ? 1 : 0;
            else
                next_grid[y][x] = (n == 3) ? 1 : 0;
        }
    }
}

/* ------------------------------------------------------------------ */

void conway_init(void) {
    do_seed();
    last_step_ms = 0;
    stasis_ms    = 0;
    draw_grid();
}

void conway_update(unsigned long now_ms) {
    if (stasis_ms) {
        if (now_ms - stasis_ms >= RESEED_MS) {
            do_seed();
            last_step_ms = now_ms;
            draw_grid();
        }
        return;
    }

    if (now_ms - last_step_ms < GEN_MS) return;
    last_step_ms = now_ms;

    compute_next();

    /* Still life: nothing changed — display is already correct. */
    if (memcmp(grid, next_grid, sizeof(grid)) == 0) {
        stasis_ms = now_ms;
        return;
    }

    memcpy(grid, next_grid, sizeof(grid));
    draw_grid();

    /* Extinction: all cells died in this step — display the all-dark grid,
       then wait before re-seeding just like a still life. */
    if (!grid_has_live(grid)) {
        stasis_ms = now_ms;
    }
}

void conway_on_input(InputEvent ev) {
    (void)ev;  /* screensaver ignores input — handled by main loop */
}

int conway_is_over(void) {
    return 0;  /* screensaver never ends on its own */
}

/* ------------------------------------------------------------------ */

void conway_clear_grid(void) {
    memset(grid, 0, sizeof(grid));
    stasis_ms    = 0;
    last_step_ms = 0;
}

void conway_set_cell(uint8_t x, uint8_t y, int alive) {
    if (x < COLS && y < ROWS)
        grid[y][x] = alive ? 1 : 0;
}

int conway_get_cell(uint8_t x, uint8_t y) {
    if (x >= COLS || y >= ROWS) return 0;
    return grid[y][x];
}

int conway_in_stasis(void) {
    return stasis_ms != 0;
}

/* ------------------------------------------------------------------ */

const Game conway_game = {
    "Conway's Game of Life",
    conway_init,
    conway_update,
    conway_on_input,
    conway_is_over
};
