#ifndef INPUT_H_
#define INPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_NONE,
    INPUT_TAP,
    INPUT_DOUBLE_TAP,
    INPUT_HOLD
} InputEvent;

void          input_init(void);
InputEvent    input_poll(void);
unsigned long input_last_event_ms(void);

#ifdef __cplusplus
}
#endif

#endif
