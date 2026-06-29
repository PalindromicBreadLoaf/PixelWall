#ifndef CONWAY_H_
#define CONWAY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "game.h"

// Game interface used by the app loop.
void conway_init(void);
void conway_update(unsigned long now_ms);
void conway_on_input(InputEvent ev);
int  conway_is_over(void);

extern const Game conway_game;

// Test and seed helpers.
void conway_clear_grid(void);
void conway_set_cell(uint8_t x, uint8_t y, int alive);
int  conway_get_cell(uint8_t x, uint8_t y);
int  conway_in_stasis(void);

#ifdef __cplusplus
}
#endif

#endif
