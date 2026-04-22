# Pixel Wall Game System — Implementation Plan

## Hardware

- **Board**: Arduino Mega 2560
- **Display**: 10 WS2812B panels, 5×5 LEDs each, arranged 2 wide × 5 tall
  - Logical resolution: **10px wide × 25px tall** (portrait)
  - LED data line: **pin 7** (via 250Ω resistor, per schematic)
  - Power: 24V from wall → manifold → panels; Arduino powered separately at 5V
- **Input**: Single momentary pushbutton
  - Recommended pin: **pin 2** (interrupt-capable on Mega)
  - The current `Tetris.ino` uses `TinyIRReceiver` which must be replaced entirely

---

## File Structure

```
Tetris/
├── Tetris.ino           # Arduino entry point: setup/loop, mode switching, screensaver
├── display.h            # Display API (shared by Arduino and simulator)
├── display.c            # Arduino implementation (FastLED + transform())
├── input.h              # Input API (shared)
├── input.c              # Arduino implementation (digitalRead + debounce)
├── game.h               # Game interface struct
├── tetris.h / tetris.c
├── stacker.h / stacker.c
├── space_invaders.h / space_invaders.c
├── snake.h / snake.c
├── conway.h / conway.c  # Screensaver only
└── simulator/
    ├── main.c           # SDL2 window + keyboard input
    ├── display_sim.c    # Simulated display (replaces display.c)
    ├── input_sim.c      # Keyboard input (replaces input.c)
    └── Makefile
```

All game logic (`.c` files) is plain C, no Arduino-specific headers. The `.ino` and `display.c`/`input.c` are the only Arduino-coupled files.

---

## Layer 1: Display Abstraction

**`display.h`**
```c
#define DISPLAY_W 10
#define DISPLAY_H 25

void display_set(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
void display_get(uint8_t x, uint8_t y, uint8_t *r, uint8_t *g, uint8_t *b);
void display_clear(void);
void display_show(void);
```

**Arduino (`display.c`)**: wraps existing `leds[]` array and `transform()` function.  
**Simulator (`display_sim.c`)**: renders to SDL2 — each LED is a ~20×20px square with 3px black gaps between pixels and slightly larger gaps between 5×5 panel boundaries.

Games never access `leds[]`, `transform()`, or `FastLED` directly.

---

## Layer 2: Input Abstraction

**`input.h`**
```c
typedef enum {
    INPUT_NONE,
    INPUT_TAP,          // press < 300ms
    INPUT_DOUBLE_TAP,   // two taps < 400ms apart
    INPUT_HOLD          // press >= 1000ms
} InputEvent;

void    input_init(void);
InputEvent input_poll(void);   // call once per frame
bool    input_any_since(unsigned long timestamp_ms);
```

On Arduino (`input.c`): `digitalRead()` on pin 2 with software debounce (10ms).  
In simulator (`input_sim.c`): Space = tap, D = double-tap, H = hold (game switch).

`input_any_since()` is used by the main loop for screensaver timeout.

---

## Layer 3: Game Interface

**`game.h`**
```c
typedef struct {
    const char *name;
    void (*init)(void);
    void (*update)(unsigned long now_ms);
    void (*on_input)(InputEvent ev);
    bool (*is_over)(void);   // true triggers restart of same game
} Game;
```

The main loop owns mode switching. Games never switch modes themselves.

---

## Main Loop (`Tetris.ino`)

```
SCREENSAVER_TIMEOUT = 30 seconds (configurable)
GAMES = [Tetris, Stacker, SpaceInvaders, Snake]
SCREENSAVER = Conway

current_game_index = 0
in_screensaver = false

loop():
    ev = input_poll()

    if ev != NONE:
        if in_screensaver:
            exit screensaver → resume current game, init it
            return
        if ev == HOLD:
            advance current_game_index (cycle through GAMES)
            init new game
            return
        else:
            current_game.on_input(ev)

    if !in_screensaver && !input_any_since(now - SCREENSAVER_TIMEOUT):
        in_screensaver = true
        conway.init()

    if in_screensaver:
        conway.update(now)
    else:
        current_game.update(now)
        if current_game.is_over():
            current_game.init()   // restart same game

    display_show()
```

---

## Games

### Tetris
- **Tap**: rotate piece clockwise
- **Double-tap**: hard drop
- **Movement**: pieces spawn centered, no left/right control — gravity only
- **Row clear**: standard; speed increases with pieces dropped
- **Game over**: piece spawns into occupied cells
- Display uses rows 0–24; top row is spawn zone

### Stacker
- A row of 3 lit pixels slides left and right across the 10px width
- **Tap**: lock the current row; active row moves up one
- Locked pixels only survive where they overlap the row below (or all survive on row 0)
- Win: reach the top with at least 1 pixel remaining
- Lose: all pixels eliminated by a miss
- Speed increases each locked row
- Display: bottom = row 24, stack grows upward

### Space Invaders (simplified)
- 3×3 grid of invaders at the top (rows 0–8), displayed as 2px dots with gaps
- Invaders drift left/right and slowly descend one row at a time
- Player: 2px wide, auto-oscillates left/right at row 24
- **Tap**: fire a bullet upward from current player position
- Bullet travels up one row per update tick
- Hit invader = remove it; all gone = win (show animation, restart harder)
- Invader reaches row 22 = game over
- One active bullet at a time

### Snake
- Snake starts 3 segments long, moving upward, spawned at center-bottom
- **Tap**: turn right relative to current direction (cycles: up→right→down→left→up)
- Food: single random lit pixel; eating it grows the snake by 1
- Collision with wall or self = game over
- Speed increases slightly with length

### Conway's Game of Life (Screensaver)
- Random initial state seeded from `analogRead()` noise
- Standard B3/S23 rules
- Detects stasis (no change between generations) or extinction → re-seeds with new random state after a 2s pause
- Any button press exits back to the active game

---

## Potential Additional Games (not in initial scope)

- **Flappy Bird variant**: bird at column 5, tap to flap (move up), gaps scroll downward through the 25 rows
- **Breakout**: ball bounces, paddle auto-oscillates, tap to temporarily reverse paddle direction
- **Reaction timer**: light descends a column, tap when it hits the target row — score is accuracy

---

## Virtual Simulator

**Build**: `cd simulator && make && ./pixel_wall`  
**Dependencies**: `gcc`, `libsdl2-dev`

The simulator compiles `../tetris.c`, `../stacker.c`, etc. directly against `display_sim.c` and `input_sim.c`. The game code is identical to what runs on the Arduino — no stubs or mocks.

**Rendering**: SDL2 window sized to fit 10×25 panel grid  
- Each LED: 20×20px colored square  
- Inter-pixel gap: 3px black  
- Inter-panel gap (every 5px): 6px black  
- Background: dark grey (#111)

**Keyboard**:
| Key   | Event         |
|-------|---------------|
| Space | Tap           |
| D     | Double-tap    |
| H     | Hold (switch) |
| Q     | Quit          |

**Timing**: `millis()` shimmed to `SDL_GetTicks()` so game speed is identical to hardware.

---

## Phased Build Order

1. `display.h` + `display_sim.c` + simulator window (verify rendering)
2. `input.h` + `input_sim.c` (verify tap/hold detection in simulator)
3. `display.c` + `input.c` for Arduino (verify on hardware)
4. Conway screensaver (simplest game, good display/timing smoke test)
5. Stacker (simplest interactive game)
6. Tetris (port from existing `.ino`, remove IR code)
7. Space Invaders
8. Snake
9. Main loop with mode switching and screensaver timer
