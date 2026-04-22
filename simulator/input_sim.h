#ifndef INPUT_SIM_H_
#define INPUT_SIM_H_

/* Returns non-zero if Q was pressed or the window close button was clicked. */
int input_sim_should_quit(void);

/* Override the hold threshold — use a small value (e.g. 50 ms) in tests
   so hold detection doesn't require a multi-second SDL_Delay. */
void input_sim_set_hold_min_ms(unsigned long ms);

#endif /* INPUT_SIM_H_ */
