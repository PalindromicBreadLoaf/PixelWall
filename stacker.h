#ifndef STACKER_H_
#define STACKER_H_

#include <stdint.h>
#include "game.h"

/* Game interface */
void stacker_init(void);
void stacker_update(unsigned long now_ms);
void stacker_on_input(InputEvent ev);
int  stacker_is_over(void);

extern const Game stacker_game;

/* Test and inspection helpers */
int  stacker_get_stack_row(void);
int  stacker_get_block_x(void);
int  stacker_get_block_width(void);
unsigned long stacker_get_speed_ms(void);
int  stacker_is_cell_locked(uint8_t x, uint8_t y);
int  stacker_won(void);
void stacker_set_block_x(int x);   /* force block position for tests */

#endif /* STACKER_H_ */
