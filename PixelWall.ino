#include <Arduino.h>
#include "display.h"
#include "input.h"
#include "app.h"

void setup() {
    /* Seed the C rand() used by Conway and Space Invaders.  A single
       analogRead gives little entropy, so mix several reads of the floating
       pin together with micros() for a more varied start each power-up. */
    unsigned long seed = micros();
    for (int i = 0; i < 16; i++)
        seed = seed * 31u + (unsigned long)analogRead(A0);
    srand((unsigned)seed);

    display_init();
    input_init();
    app_init(millis());
}

void loop() {
    InputEvent ev = input_poll();
    app_step(ev, millis());
}
