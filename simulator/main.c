#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include "../display.h"
#include "../input.h"
#include "../game.h"
#include "../conway.h"
#include "../stacker.h"
#include "../space_invaders.h"
#include "input_sim.h"

extern void display_sim_quit(void);

#define SCREENSAVER_TIMEOUT_MS 30000UL
#define WIPE_STEP_MS              30UL  /* ms per column — 10 columns = 300 ms total */
#define CURTAIN_STEP_MS           20UL  /* ms per row — 25 rows = 500 ms per phase */

static const Game *games[] = {
    &stacker_game,
    &si_game,
};
#define NUM_GAMES ((int)(sizeof(games) / sizeof(games[0])))

int main(void) {
    srand((unsigned)time(NULL));

    display_init();
    input_init();

    int current_game  = 0;
    int in_screensaver = 0;
    int in_wipe       = 0;
    int wipe_out      = 0;
    int wipe_next     = 0;
    int wipe_col      = 0;
    unsigned long last_wipe_ms = 0;

    /* Screensaver curtain: a white-row transition into/out of Conway. */
    int ss_trans = 0;   /* curtain active */
    int ss_enter = 0;   /* 1 = into screensaver, 0 = back to game */
    int ss_phase = 0;   /* 0 = fill, 1 = reveal */
    int ss_row   = 0;
    unsigned long last_ss_ms = 0;
    /* Snapshot of the scene being revealed, so the curtain rolls over a
       stable image instead of the scene's sparse per-tick redraws. */
    uint8_t scene_buf[DISPLAY_H][DISPLAY_W][3];

    games[current_game]->init();

    while (!input_sim_should_quit()) {
        InputEvent ev = input_poll();
        unsigned long now = (unsigned long)SDL_GetTicks();

        if (ev != INPUT_NONE) {
            if (ss_trans) {
                /* ignore input while the screensaver curtain is animating */
            } else if (in_screensaver) {
                /* roll the curtain back to reveal the current game */
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

        if (!in_screensaver && !ss_trans && !in_wipe
                && input_last_event_ms() != 0
                && now - input_last_event_ms() > SCREENSAVER_TIMEOUT_MS) {
            /* drop the curtain on the way into the screensaver */
            ss_trans   = 1;
            ss_enter   = 1;
            ss_phase   = 0;
            ss_row     = 0;
            last_ss_ms = now;
        }

        if (ss_trans) {
            if (ss_phase == 0) {
                /* FILL: paint one white row per step.  Enter fills bottom→top,
                   exit fills top→bottom.  The frozen scene shows underneath. */
                if (now - last_ss_ms >= CURTAIN_STEP_MS) {
                    last_ss_ms = now;
                    int row = ss_enter ? (DISPLAY_H - 1 - ss_row) : ss_row;
                    for (int x = 0; x < DISPLAY_W; x++)
                        display_set((uint8_t)x, (uint8_t)row, 255, 255, 255);
                    ss_row++;
                    if (ss_row >= DISPLAY_H) {
                        /* Screen is fully white.  Build the scene to reveal and
                           snapshot it, then repaint white so init()'s draw
                           doesn't flash before the reveal begins. */
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
                /* REVEAL: restore the snapshot, then cover the still-hidden
                   rows with white.  Enter rolls white down from the top,
                   exit rolls it back up from the bottom. */
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
                /* WIPE_IN: paint one white column per step, left → right. */
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
                        /* Repaint white so init()'s draw doesn't show this frame. */
                        for (int x = 0; x < DISPLAY_W; x++)
                            for (int y = 0; y < DISPLAY_H; y++)
                                display_set((uint8_t)x, (uint8_t)y, 255, 255, 255);
                    }
                }
            } else {
                /* WIPE_OUT: run the new game each frame so revealed columns
                   show a live image, then re-paint white over the unrevealed
                   right-side columns on top. */
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
        SDL_Delay(16);
    }

    display_sim_quit();
    return 0;
}
