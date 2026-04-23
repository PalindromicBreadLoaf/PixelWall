#ifndef SPACE_INVADERS_H_
#define SPACE_INVADERS_H_

#include <stdint.h>
#include "game.h"

void si_init(void);
void si_update(unsigned long now_ms);
void si_on_input(InputEvent ev);
int  si_is_over(void);

extern const Game si_game;

/* Test and inspection helpers */
int  si_get_player_x(void);
int  si_get_player_frozen(void);
int  si_get_bullet_x(void);
int  si_get_bullet_y(void);     /* -1 = no active bullet */
int  si_get_inv_bullet_x(void);
int  si_get_inv_bullet_y(void); /* -1 = no active bullet */
int  si_get_group_x(void);
int  si_get_group_y(void);
int  si_get_level(void);
int  si_get_alive_count(void);
int  si_get_invader_alive(int row, int col);
int  si_in_end_state(void);

void si_kill_invader(int row, int col);
void si_set_group_y(int y);
void si_set_bullet(int x, int y);
void si_set_inv_bullet(int x, int y);

#endif /* SPACE_INVADERS_H_ */
