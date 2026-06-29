#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_W 10
#define DISPLAY_H 25

void display_init(void);

// Set pixel at (x, y) to RGB. Out-of-bounds writes are silently ignored.
void display_set(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

// Get pixel color at (x, y). Out-of-bounds reads return (0,0,0).
void display_get(uint8_t x, uint8_t y, uint8_t *r, uint8_t *g, uint8_t *b);

void display_clear(void);

void display_show(void);

#ifdef __cplusplus
}
#endif

#endif
