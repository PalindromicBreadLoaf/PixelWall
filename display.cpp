// FastLED display backend.
#include "display.h"
#include <Arduino.h>
#include <FastLED.h>

#define DATA_PIN  7
#define PANEL_W   5
#define PANEL_H   5
#define PANELS_X  2
#define PANELS_Y  5
#define NUM_LEDS  (DISPLAY_W * DISPLAY_H)

static CRGB leds[NUM_LEDS];

// Convert logical (x, y) to the LED index.
static short transform_pixel(int x, int y) {
    if (x < 0 || x >= DISPLAY_W || y < 0 || y >= DISPLAY_H) return -1;

    static const uint8_t CONVERT[PANEL_W * PANEL_H] = {
         8,  9, 16, 17, 24,
         7, 10, 15, 18, 23,
         6, 11, 14, 19, 22,
         5, 12, 13, 20, 21,
         4,  3,  2,  1,  0
    };

    uint8_t tile   = PANEL_W * PANEL_H;
    uint8_t row    = (PANELS_Y - 1) - y / PANEL_H;
    uint8_t r      = y % PANEL_H;
    uint8_t col    = x / PANEL_W;
    uint8_t c      = x % PANEL_W;

    return (short)(row * tile + tile * PANELS_Y * col + CONVERT[r * PANEL_W + c]);
}

extern "C" void display_init(void) {
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
    display_clear();
    display_show();
}

extern "C" void display_set(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    short idx = transform_pixel(x, y);
    if (idx >= 0) leds[idx].setRGB(r, g, b);
}

extern "C" void display_get(uint8_t x, uint8_t y, uint8_t *r, uint8_t *g, uint8_t *b) {
    short idx = transform_pixel(x, y);
    if (idx >= 0) {
        *r = leds[idx].r;
        *g = leds[idx].g;
        *b = leds[idx].b;
    } else {
        *r = *g = *b = 0;
    }
}

extern "C" void display_clear(void) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
}

extern "C" void display_show(void) {
    FastLED.show();
}
