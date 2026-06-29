// Arduino input backend for one active-low button.
#include "input.h"
#include <Arduino.h>

#define BUTTON_PIN        2
#define DEBOUNCE_MS      10UL
#define TAP_MAX_MS       300UL
#define DOUBLE_TAP_WIN_MS 400UL
#define HOLD_MIN_MS      2000UL

typedef enum { ST_IDLE, ST_PRESSING, ST_AFTER_TAP, ST_HELD } BtnState;

static BtnState      btn_state;
static unsigned long press_start_ms;
static unsigned long tap_release_ms;
static int           first_tap_done;
static int           last_raw;
static unsigned long debounce_start;
static int           debounced;

static unsigned long last_event_ms_val;

extern "C" void input_init(void) {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    btn_state         = ST_IDLE;
    press_start_ms    = 0;
    tap_release_ms    = 0;
    first_tap_done    = 0;
    last_raw          = HIGH;
    debounce_start    = 0;
    debounced         = 0;
    last_event_ms_val = 0;
}

static int read_button(unsigned long now) {
    int raw = !digitalRead(BUTTON_PIN);
    if (raw != last_raw) {
        last_raw       = raw;
        debounce_start = now;
    }
    if (now - debounce_start >= DEBOUNCE_MS)
        debounced = last_raw;
    return debounced;
}

extern "C" InputEvent input_poll(void) {
    unsigned long now    = millis();
    int           btn    = read_button(now);
    InputEvent    result = INPUT_NONE;

    switch (btn_state) {
    case ST_IDLE:
        if (btn) {
            btn_state      = ST_PRESSING;
            press_start_ms = now;
            first_tap_done = 0;
        }
        break;

    case ST_PRESSING:
        if (!btn) {
            unsigned long dur = now - press_start_ms;
            if (dur < TAP_MAX_MS) {
                if (first_tap_done) {
                    result         = INPUT_DOUBLE_TAP;
                    btn_state      = ST_IDLE;
                    first_tap_done = 0;
                } else {
                    result         = INPUT_TAP;
                    btn_state      = ST_AFTER_TAP;
                    tap_release_ms = now;
                }
            } else if (dur >= HOLD_MIN_MS) {
                result         = INPUT_HOLD;
                btn_state      = ST_IDLE;
                first_tap_done = 0;
            } else {
                btn_state      = ST_IDLE;
                first_tap_done = 0;
            }
        } else if (now - press_start_ms >= HOLD_MIN_MS) {
            result         = INPUT_HOLD;
            btn_state      = ST_HELD;
            first_tap_done = 0;
        }
        break;

    case ST_AFTER_TAP:
        if (btn) {
            btn_state      = ST_PRESSING;
            press_start_ms = now;
            first_tap_done = 1;
        } else if (now - tap_release_ms >= DOUBLE_TAP_WIN_MS) {
            btn_state = ST_IDLE;
        }
        break;

    case ST_HELD:
        if (!btn) btn_state = ST_IDLE;
        break;
    }

    if (result != INPUT_NONE)
        last_event_ms_val = now;

    return result;
}

extern "C" unsigned long input_last_event_ms(void) {
    return last_event_ms_val;
}
