# PixelWall

A simple collection of games made for a single button 10x25 pixelwall. 

---

# SETUP

This assumes you are using a Linux Computer/VM. I don't know how to do this on macOS/Windows, but it is possible.

### Wiring the button

The button connects between pin 2 and GND on the Arduino. That is all.

## Software Setup

These steps are done once on a Linux computer. I dunno for macOS or Windows.

### Step 1: Install `arduino-cli`

Open a terminal (everything will be done here) and run:

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
  | BINDIR=~/.local/bin sh
```

Add it to your shell's PATH:

```bash
echo 'export PATH=$PATH:~/.local/bin' >> ~/.bashrc
source ~/.bashrc
```

Check it worked:

```bash
arduino-cli version
```

If you see "command not found", log out and back in.
If it still doesn't work, then start searching the internet.

---

### Step 2: Install the Arduino board support

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
```

---

### Step 3: Install the FastLED library

```bash
arduino-cli lib install FastLED
```

---

### Step 4: Allow your user to use the USB port

Add yourself to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
```

Log out for this to take effect

---

## Building and Uploading the Code

### Step 5: Connect the board

Plug the SBC into your computer. Then find out which port it appeared on:

```bash
arduino-cli board list
```

You should see a line like:

```
/dev/ttyUSB0   Serial Port   arduino:avr:mega   Arduino Mega or Mega 2560
```

Make a note of the port name (e.g. `/dev/ttyUSB0` or `/dev/ttyACM0`).

---

### Step 6: Compilation

Navigate to the folder that contains this project, then run:

```bash
arduino-cli compile --fqbn arduino:avr:mega PixelWall
```

`--fqbn arduino:avr:mega` is the Mega 2560. Change this if another board is used.

---

### Step 7: Upload to the board

Replace `/dev/ttyUSB0` below with whatever port you found in Step 5:

```bash
arduino-cli upload --fqbn arduino:avr:mega --port /dev/ttyUSB0 PixelWall
```

The Arduino's built-in LED will flicker while uploading.

> You can now compile and upload in one command in the future:
> ```bash
> arduino-cli compile --fqbn arduino:avr:mega --upload --port /dev/ttyUSB0 Tetris
> ```

---

## Playing the Games

The wall starts on Stacker.

### Button controls

| Action | How |
|--------|-----|
| **Tap** | Press and release quickly (under 0.3 seconds) |
| **Double-tap** | Two quick taps within 0.4 seconds of each other |
| **Hold** | Press and hold for 2 full seconds, then release |

### Switching games

Hold the button (2 seconds) to cycle to the next game. The order is:

1. Stacker
2. Space Invaders

This can easily be expanded to account for new games being added.

### Screensaver

If the button is not pressed for 30 seconds, Conway's Game of Life starts playing on the wall automatically. 
Press the button to return to the game it was previously on.

---

## Games
Hold to switch between games

### Stacker
Rows of blocks slide left and right. Top to lock a row in place. Only the part that overlaps the row below survives. Stack all the way to the top to win.
The blocks get faster as you go. May need to adjust forgiveness if it's too hard or easy.

### Space Invaders
Rows of invaders march down the screen. Your player slides back and forth
automatically along the bottom row.

| Action | Control |
|--------|---------|
| Fire a bullet | **Tap** |
| Stop/Start moving | **Double-tap** |

Shoot all invaders to complete a level. There are 4 levels. Each one is faster and invaders fire back more often. If an invader reaches the bottom,
or an invader bullet hits you, the game is over.

### Conway's Game of Life
Just a screensaver. It restarts automatically when it gets stuck in a loop.

---

