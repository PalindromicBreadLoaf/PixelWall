#ifndef INPUT_SIM_H_
#define INPUT_SIM_H_

int input_sim_should_quit(void);

// Tests can shorten the hold delay.
void input_sim_set_hold_min_ms(unsigned long ms);

#endif
