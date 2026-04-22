#include "display_buf.h"
#include <string.h>

uint8_t display_fb[DISPLAY_H][DISPLAY_W][3];

void display_buf_init(void) {
    memset(display_fb, 0, sizeof(display_fb));
}

void display_set(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= DISPLAY_W || y >= DISPLAY_H) return;
    display_fb[y][x][0] = r;
    display_fb[y][x][1] = g;
    display_fb[y][x][2] = b;
}

void display_get(uint8_t x, uint8_t y, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (x >= DISPLAY_W || y >= DISPLAY_H) {
        *r = *g = *b = 0;
        return;
    }
    *r = display_fb[y][x][0];
    *g = display_fb[y][x][1];
    *b = display_fb[y][x][2];
}

void display_clear(void) {
    memset(display_fb, 0, sizeof(display_fb));
}
