#ifndef APP_H_
#define APP_H_

#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared application loop, used by both the Arduino sketch and the simulator.
   The two entry points differ only in RNG seeding, the per-frame delay, and
   the quit check; everything else — game rotation, the screensaver curtain,
   and the wipe transition — lives here so it can be maintained (and tested)
   in one place.

   Both backends must call display_init() and input_init() before app_init().

   app_init() records now_ms as the baseline for the idle/screensaver timer, so
   the screensaver fires after the timeout even with no input. app_step() runs
   one frame: it consumes a polled InputEvent, advances the active game or
   transition, and pushes the frame with display_show(). */
void app_init(unsigned long now_ms);
void app_step(InputEvent ev, unsigned long now_ms);

/* Inspection helpers for tests (mirrors the per-game test helpers). */
int app_in_screensaver(void);   /* 1 while the Conway screensaver is running   */
int app_in_transition(void);    /* 1 while the screensaver curtain is animating */
int app_current_game(void);     /* index of the active game in the rotation     */
int app_num_games(void);        /* number of games in the rotation              */

#ifdef __cplusplus
}
#endif

#endif /* APP_H_ */
