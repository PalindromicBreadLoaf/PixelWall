#include "space_invaders.h"
#include "display.h"
#include "eeprom.h"
#include <stdlib.h>

#define INV_COLS        4
#define INV_ROWS        3
#define INV_X_SPACING   2   /* pixels between invader centres */
#define INV_Y_SPACING   2
#define PLAYER_ROW      (DISPLAY_H - 1)   /* row 24 */

#define PLAYER_SPEED_MS  200UL
#define BULLET_SPEED_MS   60UL
#define INV_BULLET_MS    160UL
#define END_DELAY_MS     1500UL
#define LEVEL_DELAY_MS   1200UL

/* Level indicator shown after each wave is cleared. */
#define LEVEL_INTRO_MS      1500UL
#define LEVEL_INTRO_STEP_MS  300UL   /* ms between successive dots appearing */

#define N_CONFETTI       12
#define CONFETTI_STEP_MS 120UL

static const uint8_t CONF_COLS[7][3] = {
    {255,  50,  50},
    {255, 160,   0},
    {220, 220,   0},
    {  0, 255,  80},
    {  0, 200, 255},
    { 80,  80, 255},
    {220,   0, 255},
};
static const uint8_t GREEN_COLS[3][3] = {
    {  0, 255,  80},
    { 80, 255, 140},
    {  0, 200,  50},
};
#define N_CONF_COLS  7
#define N_GREEN_COLS 3

static struct { uint8_t x, y, r, g, b; } confetti[N_CONFETTI];
static unsigned long last_confetti_ms;
#define MAX_LEVELS         4

#define END_NONE      0
#define END_LEVEL_WIN 1
#define END_GAME_WIN  2
#define END_GAME_OVER 3

#define MAX_BULLETS 3

/* Per-level tuning (index = level-1). */
static const unsigned long INV_MOVE_MS[MAX_LEVELS] = {900, 650, 450, 300};
static const unsigned long INV_FIRE_MS[MAX_LEVELS]  = {3000, 2400, 1800, 1300};

/* Invader row colours: top=magenta, mid=cyan, bottom=yellow. */
static const uint8_t ROW_R[INV_ROWS] = {200,   0, 200};
static const uint8_t ROW_G[INV_ROWS] = {  0, 200, 200};
static const uint8_t ROW_B[INV_ROWS] = {200, 200,   0};

/* EEPROM address for Space Invaders high level. */
#define SI_EEPROM_ADDR 0

/* ------------------------------------------------------------------ */

static uint8_t      alive[INV_ROWS][INV_COLS];
static int          group_x;
static int          group_y;
static int          inv_dir;
static unsigned long last_inv_move;

static int          player_x;
static int          player_dir;
static unsigned long last_player_move;

static struct {
    int x, y;
    unsigned long last_move;
} bullets[MAX_BULLETS];  /* y == -1 means inactive */

static int          inv_bul_x;      /* -1 = inactive */
static int          inv_bul_y;
static unsigned long last_inv_bul_move;
static unsigned long last_inv_fire;

static int           player_frozen;  /* 1 = auto-oscillation paused */
static int           level;
static int           end_type;
static unsigned long end_time_ms;
static unsigned long current_ms;

/* level_intro_start_ms > 0 while the between-wave level indicator is shown.
   The normal game loop is paused during this window. */
static unsigned long level_intro_start_ms;

#define EXPLOSION_FRAMES   4
#define EXPLOSION_FRAME_MS 80UL

static int           explosion_frame;
static unsigned long last_explosion_ms;

/* ------------------------------------------------------------------ */

static int count_alive(void) {
    int n = 0;
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (alive[r][c]) n++;
    return n;
}

static void init_confetti_rainbow(void) {
    for (int i = 0; i < N_CONFETTI; i++) {
        int ci = (i * 3) % N_CONF_COLS;
        confetti[i].x = (uint8_t)(rand() % DISPLAY_W);
        confetti[i].y = (uint8_t)(rand() % DISPLAY_H);
        confetti[i].r = CONF_COLS[ci][0];
        confetti[i].g = CONF_COLS[ci][1];
        confetti[i].b = CONF_COLS[ci][2];
    }
    last_confetti_ms = 0;
}

static void init_confetti_green(void) {
    for (int i = 0; i < N_CONFETTI; i++) {
        int ci = i % N_GREEN_COLS;
        confetti[i].x = (uint8_t)(rand() % DISPLAY_W);
        confetti[i].y = (uint8_t)(rand() % DISPLAY_H);
        confetti[i].r = GREEN_COLS[ci][0];
        confetti[i].g = GREEN_COLS[ci][1];
        confetti[i].b = GREEN_COLS[ci][2];
    }
    last_confetti_ms = 0;
}

static void draw_explosion(void) {
    int px = player_x;
    display_clear();
    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            if (!alive[r][c]) continue;
            int x = group_x + c * INV_X_SPACING;
            int y = group_y + r * INV_Y_SPACING;
            display_set((uint8_t)x, (uint8_t)y, ROW_R[r], ROW_G[r], ROW_B[r]);
        }
    }
    switch (explosion_frame) {
        case 0:
            display_set((uint8_t)px, PLAYER_ROW, 255, 255, 200);
            break;
        case 1:
            if (px > 0)           display_set((uint8_t)(px-1), PLAYER_ROW,   255, 120, 0);
            display_set((uint8_t)px, PLAYER_ROW, 255, 80, 0);
            if (px < DISPLAY_W-1) display_set((uint8_t)(px+1), PLAYER_ROW,   255, 120, 0);
            display_set((uint8_t)px, PLAYER_ROW-1, 255, 100, 0);
            break;
        case 2:
            if (px > 1)           display_set((uint8_t)(px-2), PLAYER_ROW,   180, 40, 0);
            if (px > 0)           display_set((uint8_t)(px-1), PLAYER_ROW,   120, 30, 0);
            if (px < DISPLAY_W-1) display_set((uint8_t)(px+1), PLAYER_ROW,   120, 30, 0);
            if (px < DISPLAY_W-2) display_set((uint8_t)(px+2), PLAYER_ROW,   180, 40, 0);
            display_set((uint8_t)px, PLAYER_ROW-1, 100, 30, 0);
            if (px > 0)           display_set((uint8_t)(px-1), PLAYER_ROW-2, 120, 40, 0);
            display_set((uint8_t)px, PLAYER_ROW-2, 80, 20, 0);
            if (px < DISPLAY_W-1) display_set((uint8_t)(px+1), PLAYER_ROW-2, 120, 40, 0);
            break;
        case 3:
            if (px > 2)           display_set((uint8_t)(px-3), PLAYER_ROW,   60, 10, 0);
            if (px > 1)           display_set((uint8_t)(px-2), PLAYER_ROW,   40, 10, 0);
            if (px < DISPLAY_W-2) display_set((uint8_t)(px+2), PLAYER_ROW,   40, 10, 0);
            if (px < DISPLAY_W-3) display_set((uint8_t)(px+3), PLAYER_ROW,   60, 10, 0);
            display_set((uint8_t)px, PLAYER_ROW-2, 50, 15, 0);
            display_set((uint8_t)px, PLAYER_ROW-3, 30, 10, 0);
            break;
        default: break;
    }
}

static void trigger_end(int type) {
    end_type          = type;
    end_time_ms       = current_ms;
    explosion_frame   = 0;
    last_explosion_ms = current_ms;
    if (type == END_GAME_WIN)   init_confetti_rainbow();
    if (type == END_LEVEL_WIN)  init_confetti_green();
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

    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].y >= 0)
            display_set((uint8_t)bullets[i].x, (uint8_t)bullets[i].y, 0, 255, 100);

    if (inv_bul_y >= 0)
        display_set((uint8_t)inv_bul_x, (uint8_t)inv_bul_y, 255, 50, 0);
}

/* Show level-N indicator: dots grow upward from the screen centre, one per
   LEVEL_INTRO_STEP_MS, until all N are visible. */
static void draw_level_intro(void) {
    display_clear();
    unsigned long elapsed = current_ms - level_intro_start_ms;
    int dots = (int)(elapsed / LEVEL_INTRO_STEP_MS) + 1;
    if (dots > level) dots = level;
    int cy = DISPLAY_H / 2;  /* row 12 */
    for (int i = 0; i < dots; i++)
        display_set(DISPLAY_W / 2, (uint8_t)(cy - i), 255, 200, 0);
}

static void draw_end_frame(void) {
    if (end_type == END_GAME_OVER) {
        if (explosion_frame < EXPLOSION_FRAMES) {
            if (current_ms - last_explosion_ms >= EXPLOSION_FRAME_MS) {
                last_explosion_ms = current_ms;
                explosion_frame++;
            }
            draw_explosion();
        } else {
            display_clear();
            for (int r = 0; r < INV_ROWS; r++) {
                for (int c = 0; c < INV_COLS; c++) {
                    if (!alive[r][c]) continue;
                    int x = group_x + c * INV_X_SPACING;
                    int y = group_y + r * INV_Y_SPACING;
                    display_set((uint8_t)x, (uint8_t)y, ROW_R[r], ROW_G[r], ROW_B[r]);
                }
            }
        }
        return;
    }
    /* WIN states: twinkling confetti. */
    const uint8_t (*palette)[3] = (end_type == END_GAME_WIN) ? CONF_COLS : GREEN_COLS;
    int n_pal = (end_type == END_GAME_WIN) ? N_CONF_COLS : N_GREEN_COLS;
    if (current_ms - last_confetti_ms >= CONFETTI_STEP_MS) {
        last_confetti_ms = current_ms;
        for (int i = 0; i < 3; i++) {
            int idx = rand() % N_CONFETTI;
            int ci  = rand() % n_pal;
            confetti[idx].x = (uint8_t)(rand() % DISPLAY_W);
            confetti[idx].y = (uint8_t)(rand() % DISPLAY_H);
            confetti[idx].r = palette[ci][0];
            confetti[idx].g = palette[ci][1];
            confetti[idx].b = palette[ci][2];
        }
    }
    display_clear();
    for (int i = 0; i < N_CONFETTI; i++)
        display_set(confetti[i].x, confetti[i].y,
                    confetti[i].r, confetti[i].g, confetti[i].b);
}

/* ------------------------------------------------------------------ */

static void check_bullet_hit(int i) {
    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            if (!alive[r][c]) continue;
            if (bullets[i].x == group_x + c * INV_X_SPACING &&
                bullets[i].y == group_y + r * INV_Y_SPACING) {
                alive[r][c]  = 0;
                bullets[i].y = -1;
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

    for (int i = 0; i < MAX_BULLETS; i++) { bullets[i].y = -1; bullets[i].last_move = 0; }
    inv_bul_x = inv_bul_y = -1;
    last_inv_bul_move = 0;
    last_inv_fire     = 0;

    end_type          = END_NONE;
    end_time_ms       = 0;
    last_confetti_ms  = 0;
    explosion_frame   = 0;
    last_explosion_ms = 0;
    player_frozen     = 0;
    level_intro_start_ms = 0;

    /* Persist the highest level reached. */
    uint8_t hi = eeprom_read(SI_EEPROM_ADDR);
    if (hi == 0xFF || level > hi)
        eeprom_write(SI_EEPROM_ADDR, (uint8_t)level);

    draw();
}

/* ------------------------------------------------------------------ */

void si_init(void) {
    level       = 1;
    player_x    = 0;
    player_dir  = 1;
    last_player_move = 0;
    current_ms  = 0;
    level_intro_start_ms = 0;
    init_level();
}

void si_update(unsigned long now_ms) {
    current_ms = now_ms;

    if (end_type != END_NONE) {
        draw_end_frame();
        if (end_type == END_LEVEL_WIN && now_ms - end_time_ms >= LEVEL_DELAY_MS) {
            level++;
            init_level();
            level_intro_start_ms = now_ms;  /* show level indicator */
        }
        return;
    }

    /* Show level indicator between waves. */
    if (level_intro_start_ms != 0) {
        if (now_ms - level_intro_start_ms < LEVEL_INTRO_MS) {
            draw_level_intro();
            return;
        }
        level_intro_start_ms = 0;  /* intro complete */
    }

    /* Move player (skipped while frozen). */
    if (!player_frozen && now_ms - last_player_move >= PLAYER_SPEED_MS) {
        last_player_move = now_ms;
        player_x += player_dir;
        if (player_x >= DISPLAY_W)  { player_x = DISPLAY_W - 1; player_dir = -1; }
        else if (player_x < 0)      { player_x = 0;             player_dir =  1; }
    }

    /* Move player bullets. */
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].y < 0) continue;
        if (now_ms - bullets[i].last_move >= BULLET_SPEED_MS) {
            bullets[i].last_move = now_ms;
            bullets[i].y--;
            if (bullets[i].y >= 0)
                check_bullet_hit(i);
        }
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
    if (level_intro_start_ms != 0) return;  /* no input during level intro */
    if (ev == INPUT_DOUBLE_TAP) {
        player_frozen = !player_frozen;
    } else if (ev == INPUT_TAP) {
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].y < 0) {
                bullets[i].x         = player_x;
                bullets[i].y         = PLAYER_ROW - 1;
                bullets[i].last_move = current_ms;
                break;
            }
        }
    }
}

int si_is_over(void) {
    return (end_type == END_GAME_OVER || end_type == END_GAME_WIN)
           && (current_ms - end_time_ms >= END_DELAY_MS);
}

/* ------------------------------------------------------------------ */

int  si_get_player_x(void)              { return player_x;      }
int  si_get_player_frozen(void)         { return player_frozen;  }
int  si_get_bullet_x(void)              { return bullets[0].x; }
int  si_get_bullet_y(void)              { return bullets[0].y; }
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
void si_set_bullet(int x, int y)   { bullets[0].x = x; bullets[0].y = y; }
void si_set_inv_bullet(int x, int y) { inv_bul_x = x; inv_bul_y = y; }

/* ------------------------------------------------------------------ */

const Game si_game = {
    "Space Invaders",
    si_init,
    si_update,
    si_on_input,
    si_is_over
};
