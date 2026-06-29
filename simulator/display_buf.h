#ifndef DISPLAY_BUF_H
#define DISPLAY_BUF_H

#include "../display.h"

// Raw framebuffer: [y][x][channel], where channel is RGB.
extern uint8_t display_fb[DISPLAY_H][DISPLAY_W][3];

void display_buf_init(void);

#endif
