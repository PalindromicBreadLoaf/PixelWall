#include <Arduino.h>
#include "display.h"
#include "input.h"
#include "app.h"

void setup() {
    // Mix a floating analog pin into the random seed.
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
