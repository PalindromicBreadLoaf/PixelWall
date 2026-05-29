#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include "../display.h"
#include "../input.h"
#include "../app.h"
#include "input_sim.h"

extern void display_sim_quit(void);

int main(void) {
    srand((unsigned)time(NULL));

    display_init();
    input_init();
    app_init((unsigned long)SDL_GetTicks());

    while (!input_sim_should_quit()) {
        InputEvent ev = input_poll();
        app_step(ev, (unsigned long)SDL_GetTicks());
        SDL_Delay(16);
    }

    display_sim_quit();
    return 0;
}
