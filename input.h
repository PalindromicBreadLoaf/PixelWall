#ifndef INPUT_H_
#define INPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_NONE,
    INPUT_TAP,         /* press released in < 300 ms */
    INPUT_DOUBLE_TAP,  /* two taps within 400 ms */
    INPUT_HOLD         /* press held >= 1000 ms */
} InputEvent;

void          input_init(void);
InputEvent    input_poll(void);          /* call once per frame */
unsigned long input_last_event_ms(void); /* millis of last non-NONE event */

#ifdef __cplusplus
}
#endif

#endif /* INPUT_H_ */
