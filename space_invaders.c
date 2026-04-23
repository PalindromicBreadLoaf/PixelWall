#include "space_invaders.h"
#include "display.h"
#include <stdlib.h>
#include <string.h>

#define INV_COLS        4
#define INV_ROWS        3
#define INV_X_SPACING   2   /* pixels between invader centres */
#define INV_Y_SPACING   2
#define PLAYER_ROW      (DISPLAY_H - 1)   /* row 24 */

#define PLAYER_SPEED_MS  200UL
#define BULLET_SPEED_MS   60UL
#define INV_BULLET_MS    160UL
#define END_DELAY_MS    1500UL  /* game-over / game-win flash duration */
#define LEVEL_DELAY_MS  1200UL  /* celebration before next level starts */
#define MAX_LEVELS         4

#define END_NONE      0
#define END_LEVEL_WIN 1
#define END_GAME_WIN  2
#define END_GAME_OVER 3

/* Per-level tuning (index = level-1). */
static const unsigned long INV_MOVE_MS[MAX_LEVELS] = {600, 430, 300, 200};
static const unsigned long INV_FIRE_MS[MAX_LEVELS]  = {3000, 2400, 1800, 1300};

/* Invader row colours: top=magenta, mid=cyan, bottom=yellow. */
static const uint8_t ROW_R[INV_ROWS] = {200,   0, 200};
static const uint8_t ROW_G[INV_ROWS] = {  0, 200, 200};
static const uint8_t ROW_B[INV_ROWS] = {200, 200,   0};

/* ------------------------------------------------------------------ */

static uint8_t      alive[INV_ROWS][INV_COLS];
static int          group_x;
static int          group_y;
static int          inv_dir;
static unsigned long last_inv_move;

static int          player_x;
static int          player_dir;
static unsigned long last_player_move;

static int          bullet_x;       /* -1 = inactive */
static int          bullet_y;
static unsigned long last_bullet_move;

static int          inv_bul_x;      /* -1 = inactive */
static int          inv_bul_y;
static unsigned long last_inv_bul_move;
static unsigned long last_inv_fire;

static int           player_frozen;  /* 1 = auto-oscillation paused */
static int           level;
static int           end_type;
static unsigned long end_time_ms;
static unsigned long current_ms;

/* ------------------------------------------------------------------ */

static int count_alive(void) {
    int n = 0;
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (alive[r][c]) n++;
    return n;
}

static void trigger_end(int type) {
    end_type    = type;
    end_time_ms = current_ms;
}

static void draw(void) {
    display_clear();

    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            if (!alive[r][c]) continue;
            int x = group_x + c * INV_X_SPACING;
            int y = group_y + r * INV_Y_SPACING;
            display_set((uint8_t)x, (uint8_t)y, ROW_R[r], ROW_G[r], ROW_B[r]);
        }
    }

    display_set((uint8_t)player_x, PLAYER_ROW, 255, 255, 255);

    if (bullet_y >= 0)
        display_set((uint8_t)bullet_x, (uint8_t)bullet_y, 0, 255, 100);

    if (inv_bul_y >= 0)
        display_set((uint8_t)inv_bul_x, (uint8_t)inv_bul_y, 255, 50, 0);
}

static void draw_end_frame(void) {
    unsigned long elapsed = current_ms - end_time_ms;
    int on = (int)((elapsed / 200) % 2);
    display_clear();
    if (on) {
        uint8_t r = 255, g = 0, b = 0;
        if (end_type == END_LEVEL_WIN)       { r=0;   g=255; b=0;   }
        else if (end_type == END_GAME_WIN)   { r=255; g=255; b=255; }
        for (int y = 0; y < DISPLAY_H; y++)
            for (int x = 0; x < DISPLAY_W; x++)
                display_set((uint8_t)x, (uint8_t)y, r, g, b);
    }
}

/* ------------------------------------------------------------------ */

static void check_bullet_hit(void) {
    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            if (!alive[r][c]) continue;
            if (bullet_x == group_x + c * INV_X_SPACING &&
                bullet_y == group_y + r * INV_Y_SPACING) {
                alive[r][c] = 0;
                bullet_y    = -1;
                if (count_alive() == 0)
                    trigger_end(level >= MAX_LEVELS ? END_GAME_WIN : END_LEVEL_WIN);
                return;
            }
        }
    }
}

static void move_invaders(void) {
    int min_c = INV_COLS, max_c = -1;
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (alive[r][c]) {
                if (c < min_c) min_c = c;
                if (c > max_c) max_c = c;
            }
    if (max_c < 0) return;

    group_x += inv_dir;

    int left_x  = group_x + min_c * INV_X_SPACING;
    int right_x = group_x + max_c * INV_X_SPACING;

    if (right_x >= DISPLAY_W) {
        group_x  = DISPLAY_W - 1 - max_c * INV_X_SPACING;
        inv_dir  = -1;
        group_y += 1;
    } else if (left_x < 0) {
        group_x  = -(min_c * INV_X_SPACING);
        inv_dir  = 1;
        group_y += 1;
    }

    /* Find lowest alive row to check danger zone. */
    int max_r = 0;
    for (int r = INV_ROWS - 1; r >= 0; r--) {
        for (int c = 0; c < INV_COLS; c++) {
            if (alive[r][c]) { max_r = r; goto found; }
        }
    }
    found:;
    if (group_y + max_r * INV_Y_SPACING >= PLAYER_ROW - 1)
        trigger_end(END_GAME_OVER);
}

static void fire_invader_bullet(void) {
    /* Collect the bottom-most alive invader in each column. */
    int shooter_c[INV_COLS], shooter_r[INV_COLS], n = 0;
    for (int c = 0; c < INV_COLS; c++) {
        for (int r = INV_ROWS - 1; r >= 0; r--) {
            if (alive[r][c]) {
                shooter_c[n] = c;
                shooter_r[n] = r;
                n++;
                break;
            }
        }
    }
    if (n == 0) return;

    int idx  = rand() % n;
    inv_bul_x = group_x + shooter_c[idx] * INV_X_SPACING;
    inv_bul_y = group_y + shooter_r[idx] * INV_Y_SPACING + 1;
    last_inv_bul_move = current_ms;
}

/* Called internally to reset invaders for the current level. */
static void init_level(void) {
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            alive[r][c] = 1;

    group_x = 0;
    group_y = 1;
    inv_dir = 1;
    last_inv_move = 0;

    bullet_x  = bullet_y  = -1;
    inv_bul_x = inv_bul_y = -1;
    last_bullet_move  = 0;
    last_inv_bul_move = 0;
    last_inv_fire     = 0;

    end_type      = END_NONE;
    end_time_ms   = 0;
    player_frozen = 0;

    draw();
}

/* ------------------------------------------------------------------ */

void si_init(void) {
    level       = 1;
    player_x    = 0;
    player_dir  = 1;
    last_player_move = 0;
    current_ms  = 0;
    init_level();
}

void si_update(unsigned long now_ms) {
    current_ms = now_ms;

    if (end_type != END_NONE) {
        draw_end_frame();
        if (end_type == END_LEVEL_WIN && now_ms - end_time_ms >= LEVEL_DELAY_MS) {
            level++;
            init_level();
        }
        return;
    }

    /* Move player (skipped while frozen). */
    if (!player_frozen && now_ms - last_player_move >= PLAYER_SPEED_MS) {
        last_player_move = now_ms;
        player_x += player_dir;
        if (player_x >= DISPLAY_W)  { player_x = DISPLAY_W - 1; player_dir = -1; }
        else if (player_x < 0)      { player_x = 0;             player_dir =  1; }
    }

    /* Move player bullet. */
    if (bullet_y >= 0 && now_ms - last_bullet_move >= BULLET_SPEED_MS) {
        last_bullet_move = now_ms;
        bullet_y--;
        if (bullet_y < 0)
            bullet_y = -1;
        else
            check_bullet_hit();
    }

    /* Move invaders. */
    if (now_ms - last_inv_move >= INV_MOVE_MS[level - 1]) {
        last_inv_move = now_ms;
        move_invaders();
        if (end_type != END_NONE) { draw_end_frame(); return; }
    }

    /* Invader fires. */
    if (inv_bul_y < 0 && now_ms - last_inv_fire >= INV_FIRE_MS[level - 1]) {
        last_inv_fire = now_ms;
        fire_invader_bullet();
    }

    /* Move invader bullet. */
    if (inv_bul_y >= 0 && now_ms - last_inv_bul_move >= INV_BULLET_MS) {
        last_inv_bul_move = now_ms;
        inv_bul_y++;
        if (inv_bul_y >= DISPLAY_H) {
            inv_bul_y = -1;
        } else if (inv_bul_y == PLAYER_ROW && inv_bul_x == player_x) {
            inv_bul_y = -1;
            trigger_end(END_GAME_OVER);
        }
    }

    draw();
}

void si_on_input(InputEvent ev) {
    if (end_type != END_NONE) return;
    if (ev == INPUT_DOUBLE_TAP) {
        player_frozen = !player_frozen;
    } else if (ev == INPUT_TAP && bullet_y < 0) {
        bullet_x         = player_x;
        bullet_y         = PLAYER_ROW - 1;
        last_bullet_move = current_ms;
    }
}

int si_is_over(void) {
    return (end_type == END_GAME_OVER || end_type == END_GAME_WIN)
           && (current_ms - end_time_ms >= END_DELAY_MS);
}

/* ------------------------------------------------------------------ */

int  si_get_player_x(void)              { return player_x;      }
int  si_get_player_frozen(void)         { return player_frozen;  }
int  si_get_bullet_x(void)              { return bullet_x;  }
int  si_get_bullet_y(void)              { return bullet_y;  }
int  si_get_inv_bullet_x(void)          { return inv_bul_x; }
int  si_get_inv_bullet_y(void)          { return inv_bul_y; }
int  si_get_group_x(void)               { return group_x;   }
int  si_get_group_y(void)               { return group_y;   }
int  si_get_level(void)                 { return level;     }
int  si_get_alive_count(void)           { return count_alive(); }
int  si_in_end_state(void)              { return end_type != END_NONE; }

int si_get_invader_alive(int row, int col) {
    if (row < 0 || row >= INV_ROWS || col < 0 || col >= INV_COLS) return 0;
    return alive[row][col];
}

void si_kill_invader(int row, int col) {
    if (row >= 0 && row < INV_ROWS && col >= 0 && col < INV_COLS)
        alive[row][col] = 0;
}

void si_set_group_y(int y)         { group_y   = y; }
void si_set_bullet(int x, int y)   { bullet_x  = x; bullet_y  = y; }
void si_set_inv_bullet(int x, int y) { inv_bul_x = x; inv_bul_y = y; }

/* ------------------------------------------------------------------ */

const Game si_game = {
    "Space Invaders",
    si_init,
    si_update,
    si_on_input,
    si_is_over
};
