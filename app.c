// Shared app loop for the Arduino sketch and simulator.
#include "app.h"
#include "display.h"
#include "game.h"
#include "conway.h"
#include "stacker.h"
#include "space_invaders.h"

#define SCREENSAVER_TIMEOUT_MS 30000UL
#define WIPE_STEP_MS              30UL  // one column per step
#define CURTAIN_STEP_MS           20UL  // one row per step

static const Game *games[] = {
    &stacker_game,
    &si_game,
};
#define NUM_GAMES ((int)(sizeof(games) / sizeof(games[0])))

static int current_game   = 0;
static int in_screensaver = 0;
static int in_wipe        = 0;
static int wipe_out       = 0;
static int wipe_next      = 0;
static int wipe_col       = 0;
static unsigned long last_wipe_ms = 0;

// Screensaver curtain state.
static int ss_trans = 0;   // 1 = curtain currently active
static int ss_enter = 0;   // 1 = into screensaver, 0 = back to game
static int ss_phase = 0;   // 0 = fill, 1 = reveal
static int ss_row   = 0;
static unsigned long last_ss_ms = 0;
// Snapshot used while the curtain reveals a stable image.
// Without this the game looked really funky while being shown.
static uint8_t scene_buf[DISPLAY_H][DISPLAY_W][3];

// Used as the idle baseline when no button has been pressed yet.
static unsigned long start_ms = 0;

void app_init(unsigned long now_ms) {
    current_game   = 0;
    in_screensaver = 0;
    in_wipe        = 0;
    wipe_out       = 0;
    wipe_next      = 0;
    wipe_col       = 0;
    last_wipe_ms   = 0;
    ss_trans       = 0;
    ss_enter       = 0;
    ss_phase       = 0;
    ss_row         = 0;
    last_ss_ms     = 0;
    start_ms       = now_ms;

    games[current_game]->init();
}

void app_step(InputEvent ev, unsigned long now) {
    if (ev != INPUT_NONE) {
        if (ss_trans) {
            // Ignore input while the curtain is moving.
        } else if (in_screensaver) {
            // Start the curtain back to the current game.
            ss_trans   = 1;
            ss_enter   = 0;
            ss_phase   = 0;
            ss_row     = 0;
            last_ss_ms = now;
        } else if (ev == INPUT_HOLD) {
            wipe_out     = 0;
            wipe_next    = (current_game + 1) % NUM_GAMES;
            wipe_col     = 0;
            in_wipe      = 1;
            last_wipe_ms = now;
        } else {
            games[current_game]->on_input(ev);
        }
    }

    // Use boot time when there has not been any input yet.
    unsigned long last_active = input_last_event_ms();
    if (last_active == 0) last_active = start_ms;
    if (!in_screensaver && !ss_trans && !in_wipe
            && now - last_active > SCREENSAVER_TIMEOUT_MS) {
        // Start the curtain into the screensaver.
        ss_trans   = 1;
        ss_enter   = 1;
        ss_phase   = 0;
        ss_row     = 0;
        last_ss_ms = now;
    }

    if (ss_trans) {
        if (ss_phase == 0) {
            // Fill with white before swapping scenes.
            if (now - last_ss_ms >= CURTAIN_STEP_MS) {
                last_ss_ms = now;
                int row = ss_enter ? (DISPLAY_H - 1 - ss_row) : ss_row;
                for (int x = 0; x < DISPLAY_W; x++)
                    display_set((uint8_t)x, (uint8_t)row, 255, 255, 255);
                ss_row++;
                if (ss_row >= DISPLAY_H) {
                    // Capture the next scene while the screen is hidden.
                    if (ss_enter) conway_init();
                    else          games[current_game]->init();
                    for (int y = 0; y < DISPLAY_H; y++)
                        for (int x = 0; x < DISPLAY_W; x++)
                            display_get((uint8_t)x, (uint8_t)y,
                                        &scene_buf[y][x][0],
                                        &scene_buf[y][x][1],
                                        &scene_buf[y][x][2]);
                    for (int x = 0; x < DISPLAY_W; x++)
                        for (int y = 0; y < DISPLAY_H; y++)
                            display_set((uint8_t)x, (uint8_t)y, 255, 255, 255);
                    ss_phase   = 1;
                    ss_row     = 0;
                    last_ss_ms = now;
                }
            }
        } else {
            // Reveal the saved scene row by row.
            for (int y = 0; y < DISPLAY_H; y++)
                for (int x = 0; x < DISPLAY_W; x++)
                    display_set((uint8_t)x, (uint8_t)y,
                                scene_buf[y][x][0],
                                scene_buf[y][x][1],
                                scene_buf[y][x][2]);

            int hide_lo = ss_enter ? ss_row : 0;
            int hide_hi = ss_enter ? DISPLAY_H : (DISPLAY_H - ss_row);
            for (int y = hide_lo; y < hide_hi; y++)
                for (int x = 0; x < DISPLAY_W; x++)
                    display_set((uint8_t)x, (uint8_t)y, 255, 255, 255);

            if (now - last_ss_ms >= CURTAIN_STEP_MS) {
                last_ss_ms = now;
                ss_row++;
                if (ss_row >= DISPLAY_H) {
                    ss_trans       = 0;
                    in_screensaver = ss_enter;
                }
            }
        }
    } else if (in_wipe) {
        if (!wipe_out) {
            // Hide the old game with a white column wipe.
            if (now - last_wipe_ms >= WIPE_STEP_MS) {
                last_wipe_ms = now;
                for (int y = 0; y < DISPLAY_H; y++)
                    display_set((uint8_t)wipe_col, (uint8_t)y, 255, 255, 255);
                wipe_col++;
                if (wipe_col >= DISPLAY_W) {
                    wipe_out     = 1;
                    wipe_col     = 0;
                    current_game = wipe_next;
                    games[current_game]->init();
                    last_wipe_ms = now;
                    for (int x = 0; x < DISPLAY_W; x++)
                        for (int y = 0; y < DISPLAY_H; y++)
                            display_set((uint8_t)x, (uint8_t)y, 255, 255, 255);
                }
            }
        } else {
            // Reveal the new game, keeping unrevealed columns white.
            games[current_game]->update(now);
            for (int col = wipe_col; col < DISPLAY_W; col++)
                for (int y = 0; y < DISPLAY_H; y++)
                    display_set((uint8_t)col, (uint8_t)y, 255, 255, 255);
            if (now - last_wipe_ms >= WIPE_STEP_MS) {
                last_wipe_ms = now;
                wipe_col++;
                if (wipe_col >= DISPLAY_W)
                    in_wipe = 0;
            }
        }
    } else if (in_screensaver) {
        conway_update(now);
    } else {
        games[current_game]->update(now);
        if (games[current_game]->is_over())
            games[current_game]->init();
    }

    display_show();
}

int app_in_screensaver(void) { return in_screensaver; }
int app_in_transition(void)  { return ss_trans;       }
int app_current_game(void)   { return current_game;   }
int app_num_games(void)      { return NUM_GAMES;       }
