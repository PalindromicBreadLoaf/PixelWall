#ifndef DISPLAY_BUF_H
#define DISPLAY_BUF_H

#include "../display.h"

/* Raw framebuffer — exposed so display_sim.c can read it for rendering.
   Index as [y][x][channel] where channel 0=R, 1=G, 2=B. */
extern uint8_t display_fb[DISPLAY_H][DISPLAY_W][3];

/* Zero the framebuffer. Called by display_init() in each backend. */
void display_buf_init(void);

#endif
