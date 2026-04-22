#ifndef GAME_H_
#define GAME_H_

#include "input.h"

typedef struct {
    const char *name;
    void (*init)(void);
    void (*update)(unsigned long now_ms);
    void (*on_input)(InputEvent ev);
    int  (*is_over)(void);
} Game;

#endif /* GAME_H_ */
