#ifndef APP_H_
#define APP_H_

#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shared app loop for Arduino and simulator.
// Call display_init() and input_init() before app_init().
void app_init(unsigned long now_ms);
void app_step(InputEvent ev, unsigned long now_ms);

// Inspection helpers for tests.
int app_in_screensaver(void);
int app_in_transition(void);
int app_current_game(void);
int app_num_games(void);

#ifdef __cplusplus
}
#endif

#endif
