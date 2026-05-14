#include "conway.h"
#include "display.h"
#include <stdlib.h>
#include <string.h>

#define COLS      DISPLAY_W
#define ROWS      DISPLAY_H
#define GEN_MS    750UL   /* ms between generations */
#define RESEED_MS 2000UL  /* pause before re-seeding after stasis */

#define CELL_BRIGHTNESS 200  /* 0–255 */
#define HUE_STEP          2  /* degrees per generation (full cycle ≈ 135 s) */

static int cell_hue = 0;  /* 0..359 */

static void hue_to_rgb(int h, uint8_t *r, uint8_t *g, uint8_t *b) {
    int sector = h / 60;
    int f = h % 60;
    uint8_t v = CELL_BRIGHTNESS;
    uint8_t q = (uint8_t)(v * (60 - f) / 60);
    uint8_t t = (uint8_t)(v * f / 60);
    switch (sector) {
        case 0: *r = v; *g = t; *b = 0; break;
        case 1: *r = q; *g = v; *b = 0; break;
        case 2: *r = 0; *g = v; *b = t; break;
        case 3: *r = 0; *g = q; *b = v; break;
        case 4: *r = t; *g = 0; *b = v; break;
        default:*r = v; *g = 0; *b = q; break;
    }
}

/* Rolling hash history for oscillator detection.
   Keeps FNV-1a hashes of the last HASH_HISTORY grid states.
   If the next state matches any stored hash we have a cycle (period 1–16). */
#define HASH_HISTORY 16

static uint8_t  grid[ROWS][COLS];
static uint8_t  next_grid[ROWS][COLS];

static unsigned long last_step_ms = 0;
static unsigned long stasis_ms    = 0;

static uint32_t hash_history[HASH_HISTORY];
static int      hash_count = 0;   /* total entries recorded (not capped) */

/* ------------------------------------------------------------------ */

static uint32_t fnv1a(const uint8_t *buf, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

static void do_seed(void) {
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            grid[y][x] = (rand() % 3 == 0) ? 1 : 0;
    stasis_ms  = 0;
    hash_count = 0;
}

static void draw_grid(void) {
    uint8_t r, g, b;
    hue_to_rgb(cell_hue, &r, &g, &b);
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (grid[y][x])
                display_set((uint8_t)x, (uint8_t)y, r, g, b);
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

    /* Record current state in the rolling hash history. */
    hash_history[hash_count % HASH_HISTORY] = fnv1a(&grid[0][0], ROWS * COLS);
    hash_count++;

    compute_next();

    /* Oscillator detection: if the next state matches any stored state, it's
       a cycle.  Catches still lifes (period 1), blinkers (period 2), and any
       oscillator up to period HASH_HISTORY. */
    uint32_t h_next  = fnv1a(&next_grid[0][0], ROWS * COLS);
    int      n_check = (hash_count < HASH_HISTORY) ? hash_count : HASH_HISTORY;
    for (int i = 0; i < n_check; i++) {
        if (hash_history[i] == h_next) {
            stasis_ms = now_ms;
            return;
        }
    }

    memcpy(grid, next_grid, sizeof(grid));
    cell_hue = (cell_hue + HUE_STEP) % 360;
    draw_grid();

    /* Extinction: all cells died this step. */
    if (!grid_has_live(grid))
        stasis_ms = now_ms;
}

void conway_on_input(InputEvent ev) {
    (void)ev;
}

int conway_is_over(void) {
    return 0;
}

/* ------------------------------------------------------------------ */

void conway_clear_grid(void) {
    memset(grid, 0, sizeof(grid));
    stasis_ms    = 0;
    last_step_ms = 0;
    hash_count   = 0;
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
