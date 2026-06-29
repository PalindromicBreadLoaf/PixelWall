#include "conway.h"
#include "display.h"
#include <stdlib.h>
#include <string.h>

#define COLS      DISPLAY_W
#define ROWS      DISPLAY_H
#define GEN_MS    750UL   // ms between generations
#define RESEED_MS 2000UL  // pause after stasis
#define GEN_TRANSITION_MS 300UL  // crossfade between generations
#define RESTART_FADE_MS   (RESEED_MS / 2UL)

#define CELL_BRIGHTNESS 200  // 0–255 (Higher means greater chance of distortion too)
#define DIM_DIVISOR       3  // dim cells are 1/DIM_DIVISOR brightness
#define DIM_HUE_OFFSET  180  // dim cells use the opposite hue
#define HUE_STEP          2  // hue change per generation

static int cell_hue = 0;  // 0..359

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

// Recent grid hashes to spot whether the program is stuck or not.
#define HASH_HISTORY 16

static uint8_t  grid[ROWS][COLS];
static uint8_t  next_grid[ROWS][COLS];

static unsigned long last_step_ms = 0;
static unsigned long stasis_ms    = 0;
static unsigned long gen_transition_ms = 0;
static int gen_transition_active = 0;
static int prev_hue = 0;
static int restart_seeded = 0;
static int restart_from_prev = 0;

static uint32_t hash_history[HASH_HISTORY];
static int      hash_count = 0;

static uint32_t fnv1a(const uint8_t *buf, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

static void seed_grid(void) {
    // 25% random fill leaves room for the seeded patterns below.
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            grid[y][x] = (rand() % 4 == 0) ? 1 : 0;

    // Blinkers keep the screen moving early on.
    for (int k = 0; k < 4; k++) {
        int x = rand() % (COLS - 2);
        int y = rand() % ROWS;
        grid[y][x] = grid[y][x+1] = grid[y][x+2] = 1;
    }

    // Gliders spread activity across the wrapping grid.
    for (int k = 0; k < 3; k++) {
        int x = rand() % (COLS - 2);
        int y = rand() % (ROWS - 2);
        grid[y  ][x+1] = 1;
        grid[y+1][x+2] = 1;
        grid[y+2][x  ] = grid[y+2][x+1] = grid[y+2][x+2] = 1;
    }

    hash_count = 0;
}

static void do_seed(void) {
    seed_grid();
    stasis_ms = 0;
    gen_transition_active = 0;
    restart_seeded = 0;
    restart_from_prev = 0;
}

static uint8_t progress_255(unsigned long elapsed, unsigned long duration) {
    if (duration == 0 || elapsed >= duration) return 255;
    return (uint8_t)((elapsed * 255UL) / duration);
}

static uint8_t scale8(uint8_t value, uint8_t scale) {
    return (uint8_t)(((uint16_t)value * scale) / 255U);
}

static uint8_t blend8(uint8_t from, uint8_t to, uint8_t amount) {
    uint32_t inv = 255UL - amount;
    return (uint8_t)(((uint32_t)from * inv + (uint32_t)to * amount) / 255UL);
}

static void palette_for_hue(int hue, uint8_t live[3], uint8_t dead[3]) {
    uint8_t r, g, b;
    hue_to_rgb(hue, &r, &g, &b);
    live[0] = r;
    live[1] = g;
    live[2] = b;

    hue_to_rgb((hue + DIM_HUE_OFFSET) % 360, &r, &g, &b);
    dead[0] = (uint8_t)(r / DIM_DIVISOR);
    dead[1] = (uint8_t)(g / DIM_DIVISOR);
    dead[2] = (uint8_t)(b / DIM_DIVISOR);
}

static void draw_grid_scaled(uint8_t g[ROWS][COLS], int hue, uint8_t scale) {
    uint8_t live[3], dead[3];
    palette_for_hue(hue, live, dead);
    for (int i = 0; i < 3; i++) {
        live[i] = scale8(live[i], scale);
        dead[i] = scale8(dead[i], scale);
    }

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            uint8_t *c = g[y][x] ? live : dead;
            display_set((uint8_t)x, (uint8_t)y, c[0], c[1], c[2]);
        }
    }
}

static void draw_grid(void) {
    draw_grid_scaled(grid, cell_hue, 255);
}

static void draw_generation_transition(unsigned long now_ms) {
    uint8_t amount = progress_255(now_ms - gen_transition_ms, GEN_TRANSITION_MS);
    uint8_t old_live[3], old_dead[3];
    uint8_t new_live[3], new_dead[3];
    palette_for_hue(prev_hue, old_live, old_dead);
    palette_for_hue(cell_hue, new_live, new_dead);

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            uint8_t *old_c = next_grid[y][x] ? old_live : old_dead;
            uint8_t *new_c = grid[y][x] ? new_live : new_dead;
            display_set((uint8_t)x, (uint8_t)y,
                        blend8(old_c[0], new_c[0], amount),
                        blend8(old_c[1], new_c[1], amount),
                        blend8(old_c[2], new_c[2], amount));
        }
    }

    if (amount == 255)
        gen_transition_active = 0;
}

static void draw_restart_transition(unsigned long now_ms) {
    unsigned long elapsed = now_ms - stasis_ms;
    if (elapsed < RESTART_FADE_MS) {
        uint8_t amount = progress_255(elapsed, RESTART_FADE_MS);
        draw_grid_scaled(restart_from_prev ? next_grid : grid,
                         restart_from_prev ? prev_hue : cell_hue,
                         (uint8_t)(255 - amount));
        return;
    }

    if (!restart_seeded) {
        seed_grid();
        restart_seeded = 1;
        restart_from_prev = 0;
    }

    if (elapsed < RESEED_MS) {
        uint8_t amount = progress_255(elapsed - RESTART_FADE_MS,
                                      RESEED_MS - RESTART_FADE_MS);
        draw_grid_scaled(grid, cell_hue, amount);
        return;
    }

    stasis_ms = 0;
    restart_seeded = 0;
    restart_from_prev = 0;
    last_step_ms = now_ms;
    draw_grid();
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

void conway_init(void) {
    do_seed();
    last_step_ms = 0;
    stasis_ms    = 0;
    draw_grid();
}

void conway_update(unsigned long now_ms) {
    if (stasis_ms) {
        draw_restart_transition(now_ms);
        return;
    }

    if (gen_transition_active) {
        draw_generation_transition(now_ms);
        if (gen_transition_active)
            return;
    }

    if (now_ms - last_step_ms < GEN_MS) return;
    last_step_ms = now_ms;

    // Save this grid so the next generation can be checked for loops.
    hash_history[hash_count % HASH_HISTORY] = fnv1a(&grid[0][0], ROWS * COLS);
    hash_count++;

    compute_next();

    // A repeated next state means a still life or short loop.
    uint32_t h_next  = fnv1a(&next_grid[0][0], ROWS * COLS);
    int      n_check = (hash_count < HASH_HISTORY) ? hash_count : HASH_HISTORY;
    for (int i = 0; i < n_check; i++) {
        if (hash_history[i] == h_next) {
            stasis_ms = now_ms;
            gen_transition_active = 0;
            restart_seeded = 0;
            restart_from_prev = 0;
            draw_restart_transition(now_ms);
            return;
        }
    }

    // Swap grids so next_grid keeps the old state for the fade.
    prev_hue = cell_hue;
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            uint8_t old = grid[y][x];
            grid[y][x] = next_grid[y][x];
            next_grid[y][x] = old;
        }
    }
    cell_hue = (cell_hue + HUE_STEP) % 360;
    gen_transition_ms = now_ms;
    gen_transition_active = 1;
    draw_generation_transition(now_ms);

    if (!grid_has_live(grid)) {
        stasis_ms = now_ms;
        gen_transition_active = 0;
        restart_seeded = 0;
        restart_from_prev = 1;
        draw_restart_transition(now_ms);
    }
}

void conway_on_input(InputEvent ev) {
    (void)ev;
}

int conway_is_over(void) {
    return 0;
}

void conway_clear_grid(void) {
    memset(grid, 0, sizeof(grid));
    memset(next_grid, 0, sizeof(next_grid));
    stasis_ms    = 0;
    last_step_ms = 0;
    hash_count   = 0;
    cell_hue     = 0;
    prev_hue     = 0;
    gen_transition_ms     = 0;
    gen_transition_active = 0;
    restart_seeded        = 0;
    restart_from_prev     = 0;
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

const Game conway_game = {
    "Conway's Game of Life",
    conway_init,
    conway_update,
    conway_on_input,
    conway_is_over
};
