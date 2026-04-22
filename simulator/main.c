#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include "../display.h"
#include "../input.h"
#include "../conway.h"
#include "input_sim.h"

extern void display_sim_quit(void);

int main(void) {
    srand((unsigned)time(NULL));

    display_init();
    input_init();
    conway_init();

    while (!input_sim_should_quit()) {
        InputEvent ev = input_poll();
        conway_on_input(ev);
        conway_update((unsigned long)SDL_GetTicks());
        display_show();
        SDL_Delay(16);
    }

    display_sim_quit();
    return 0;
}
