#include <FastLED.h>
#include <TinyIRReceiver.hpp>
#define DATA_PIN 7
#define WIDTH 2
#define HEIGHT 5
#define SIZE 5
#define NUM_BLOCKS 7
#define TILE SIZE* SIZE
#define NUM_LEDS TILE* WIDTH* HEIGHT
CRGB leds[NUM_LEDS];

struct Position {
  int8_t x;
  int8_t y;
};
typedef struct Position Position;

volatile struct TinyIRReceiverCallbackDataStruct sCallbackData;

short transform(struct Position pos) {
  // Table to convert current LED to real LED.
  if (pos.x >=0 && pos.x < WIDTH * SIZE && pos.y >= 0) {
    uint8_t convert[TILE] = {
      8, 9, 16, 17, 24,
      7, 10, 15, 18, 23,
      6, 11, 14, 19, 22,
      5, 12, 13, 20, 21,
      4, 3, 2, 1, 0
    };

    uint8_t row = (HEIGHT - 1) - pos.y / SIZE;
    uint8_t rowIndex = row * TILE;
    uint8_t r = pos.y % SIZE;
    uint8_t tileRow = r * SIZE;
    uint8_t col = pos.x / SIZE;
    uint8_t colIndex = 0;
    colIndex = TILE * HEIGHT * col;
    uint8_t c = pos.x % SIZE;

    return rowIndex + colIndex + convert[tileRow + c];
  }
  return -1;
}

/**
 * Block class to manange the tetris blocks.
 */
class Block {
public:
  Block() {}
  Block(uint8_t newColor[3], bool* newShape, uint8_t newSize) {
    size = newSize;
    color = newColor;
    shape = newShape;
  }

  /**
   * Gets the pos.
   */
  struct Position getPos() {
    return pos;
  }

  /**
   * Gets the rotation.
   */
  int8_t getRotation() {
    return rotation;
  }

  /**
   * Gets the shape of the item.
   */
  bool* getShape() {
    return shape;
  }

  /**
   * Returns the size of the shape.
   */
  uint8_t getSize() {
    return size;
  }

  /**
   * Updates the pos for the block.
   * @param newPos The new pos.
   * @returns If the function ran.
   */
  bool setPos(struct Position newPos) {
    // Checks if the update can happen.
    bool canUpdate = true;
    for (uint8_t y = 0; y < size && canUpdate; y++) {
      for (uint8_t x = 0; x < size && canUpdate; x++) {
        // Prevents it from hitting itself.
        bool next = false;
        if ((newPos.x + x) >= pos.x && (newPos.x + x) < pos.x + size && (newPos.y + y) >= pos.y && (newPos.y + y) < pos.y + size) {
          next = shape[(newPos.y - pos.y + y) * size + newPos.x - pos.x + x];
        }

        short trans = transform({ newPos.x + x, newPos.y + y });
        if (trans < 0) {
          /*
          If the OldPos is offscreen check if the index of the shape is true and the newpos of that same index is offscreen, canupdate = false
          */
          canUpdate = !(shape[y * size + x] && ((newPos.x + x) < 0 || (newPos.x + x > WIDTH * SIZE - 1)));
        } else {
          canUpdate = !(!next && shape[y * size + x] && leds[trans].getAverageLight() != 0);
        }
      }
    }

    if (canUpdate) {
      // Clears and updates pos.
      clear();
      pos = newPos;
    }

    return canUpdate;
  }

  /**
   * Rotates the object.
   * @param right The direction to rotate the object.
   * @returns If the function ran.
   */
  bool rotate(bool right) {
    // Creates roated matrix.
    uint8_t updated[size * size];
    uint8_t update = 0;
    for (int8_t c = size - 1; c >= 0; c--) {
      for (uint8_t r = 0; r < size; r++) {
        uint8_t localIndex = (size * size * right + -1 * right) + (right * -2 + 1) * (r * size + c);
        updated[update] = shape[localIndex];
        update++;
      }
    }

    // Checks if the update can happen.
    bool canUpdate = true;
    for (uint8_t y = 0; y < size && canUpdate; y++) {
      for (uint8_t x = 0; x < size && canUpdate; x++) {
        short trans = transform({ pos.x + x, pos.y + y });
        if (trans < 0) {
          canUpdate = !(updated[y * size + x] && (pos.x + x < 0 || pos.x + x > WIDTH * SIZE - 1));
        } else {
          canUpdate = !(!shape[y * size + x] && updated[y * size + x] && leds[trans].getAverageLight() != 0);
        }
      }
    }

    // Preforms the update.
    if (canUpdate) {
      clear();

      // Updates the shape.
      for (uint8_t i = 0; i < size * size; i++) {
        shape[i] = updated[i];
      }

      // Tracks rotaion.
      if (right)
        rotation++;
      else
        rotation--;
      rotation %= 4;
    }

    return canUpdate;
  }

  /**
   * Resets the rotation and the position of the block.
   */
  void reset() {
    pos = {(WIDTH / 2) * SIZE - size / 2 , -size};

    // Undo rotation special.
    bool right = rotation < 0;
    uint8_t updated[size * size];
    while (rotation != 0) {
      // Creates roated matrix.
      uint8_t update = 0;
      for (int8_t c = size - 1; c >= 0; c--) {
        for (uint8_t r = 0; r < size; r++) {
          uint8_t localIndex = (size * size * right + -1 * right) + (right * -2 + 1) * (r * size + c);
          updated[update] = shape[localIndex];
          update++;
        }
      }

      // Updates the shape.
      for (uint8_t i = 0; i < size * size; i++) {
        shape[i] = updated[i];
      }

      // Tracks rotaion.
      if (right)
        rotation++;
      else
        rotation--;
    }
  }

  void clear() {
    // Clears original space.
    uint8_t tempColor[3] = { color[0], color[1], color[2] };
    color[0] = color[1] = color[2] = 0;
    draw();

    // Brings back color.
    color[0] = tempColor[0];
    color[1] = tempColor[1];
    color[2] = tempColor[2];
  }

  /**
   * Draws the block.
   */
  void draw() {
    for (uint8_t r = 0; r < size; r++) {
      for (uint8_t c = 0; c < size; c++) {
        uint8_t localIndex = r * size + c;
        if (shape[localIndex]) {
          short trans = transform({ pos.x + c, pos.y + r });
          if (trans >= 0) leds[trans].setRGB(color[0], color[1], color[2]);
        }
      }
    }
  }
private:
  uint8_t size;
  uint8_t* color;
  bool* shape;
  struct Position pos = { (WIDTH / 2) * SIZE - size / 2, -size };
  int8_t rotation = 0;
};

// Variables.
unsigned long time = 0;
uint8_t dropped = 0;
int8_t index = 0;
int8_t nextIndex = 0;
int8_t holdIndex = -1;
int8_t holdRot = 0;
Block* blocks[NUM_BLOCKS];

/**
 * Checks load with white screen.
 */
void whiteCheck() {
  for (uint8_t x = 0; x < SIZE * WIDTH; x++) {
    for (uint8_t y = 0; y < SIZE * HEIGHT; y++) {
      leds[transform({ x, y })].setRGB(255, 255, 255);
      delay(10);
      FastLED.show();
    }
  }

  delay(10000);

  for (uint8_t x = 0; x < SIZE * WIDTH; x++) {
    for (uint8_t y = 0; y < SIZE * HEIGHT; y++) {
      leds[transform({ x, y })].setRGB(0, 0, 0);
      delay(10);
      FastLED.show();
    }
  }
}

/**
 * White check but red.
 */
void redCheck() {
  for (uint8_t x = 0; x < SIZE * WIDTH; x++) {
    for (uint8_t y = 0; y < SIZE * HEIGHT; y++) {
      leds[transform({ x, y })].setRGB(255, 0, 0);
      delay(10);
      FastLED.show();
    }
  }

  delay(10000);

  for (uint8_t x = 0; x < SIZE * WIDTH; x++) {
    for (uint8_t y = 0; y < SIZE * HEIGHT; y++) {
      leds[transform({ x, y })].setRGB(0, 0, 0);
      delay(10);
      FastLED.show();
    }
  }
}

/**
 * Clears the board.
 */
void clear() {
  for (uint8_t x = 0; x < SIZE * WIDTH; x++) {
    for (uint8_t y = 0; y < SIZE * HEIGHT; y++) {
      leds[transform({ x, y })].setRGB(0, 0, 0);
    }
  }

  FastLED.show();
}

/**
 * Builds the HUD.
 */
void buildHUD() {
  leds[transform({ 4, (HEIGHT - 1) * SIZE + 1 })].setRGB(255, 255, 255);
  leds[transform({ 4, (HEIGHT - 1) * SIZE + 2 })].setRGB(255, 255, 255);
  leds[transform({ 4, (HEIGHT - 1) * SIZE + 3 })].setRGB(255, 255, 255);
  leds[transform({ 4, (HEIGHT - 1) * SIZE + 4 })].setRGB(255, 255, 255);
  leds[transform({ 5, (HEIGHT - 1) * SIZE + 1 })].setRGB(255, 255, 255);
  leds[transform({ 5, (HEIGHT - 1) * SIZE + 2 })].setRGB(255, 255, 255);
  leds[transform({ 5, (HEIGHT - 1) * SIZE + 3 })].setRGB(255, 255, 255);
  leds[transform({ 5, (HEIGHT - 1) * SIZE + 4 })].setRGB(255, 255, 255);
  for (uint8_t x = 0; x < (WIDTH * SIZE); x++) {
    leds[transform({ x, (HEIGHT - 1) * SIZE })].setRGB(255, 255, 255);
  }
  FastLED.show();
}

/**
 * Draws the next block.
 */
void drawNext() {
  // Clears next block area.
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      leds[transform({ x, (HEIGHT - 1) * SIZE + 1 + y })].setRGB(0, 0, 0);
    }
  }

  // Draws the next block.
  do {
    nextIndex = random(0, NUM_BLOCKS);
  } while (nextIndex == index);
  blocks[nextIndex]->setPos({0, (HEIGHT - 1) * SIZE + 1});
  blocks[nextIndex]->draw();
  blocks[nextIndex]->reset();
}

/**
 * Draws the hold block.
 */
void drawHold() {
  // Clears next block area.
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      leds[transform({(WIDTH - 1) * SIZE + 1 + x, (HEIGHT - 1) * SIZE + 1 + y})].setRGB(0, 0, 0);
    }
  }

  // Updates hold.
  holdIndex = index;
  holdRot = blocks[holdIndex]->getRotation();
  blocks[holdIndex]->setPos({(WIDTH - 1) * SIZE + 1, (HEIGHT - 1) * SIZE + 1});
  blocks[holdIndex]->draw();
  blocks[holdIndex]->reset();
}

/**
 * Entry point for the code.
 */
void setup() {
  // Sets up IR
  initPCIInterruptForTinyReceiver();

  // Sets up the LEDs.
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  whiteCheck();
  buildHUD();

  // Sets up blocks.
  uint8_t colors[NUM_BLOCKS][3] = {
    {0, 255, 255},
    {0, 0, 255},
    {255, 40, 0},
    {255, 128, 0},
    {0, 255, 0},
    {255, 0, 255},
    {255, 0, 0}
  };
  bool iShape[16] = {
    false, false, false, false,
    true, true, true, true,
    false, false, false, false,
    false, false, false, false
  };
  bool jShape[9] = {
    true, false, false,
    true, true, true,
    false, false, false
  };
  bool lShape[9] = {
    false, false, true,
    true, true, true,
    false, false, false
  };
  bool oShape[4] = {
    true, true,
    true, true
  };
  bool sShape[9] = {
    false, true, true,
    true, true, false,
    false, false, false
  };
  bool tShape[9] = {
    false, true, false,
    true, true, true,
    false, false, false
  };
  bool zShape[9] = {
    true, true, false,
    false, true, true,
    false, false, false
  };
  bool* shapes[NUM_BLOCKS] = {
    iShape,
    jShape,
    lShape,
    oShape,
    sShape,
    tShape,
    zShape
  };
  uint8_t sizes[NUM_BLOCKS] = {4, 3, 3, 2, 3, 3, 3};
  for(uint8_t i = 0; i < NUM_BLOCKS; i++) {
    blocks[i] = new Block(colors[i], shapes[i], sizes[i]);
  }

  // Gets starting index.
  randomSeed(analogRead(5));
  index = random(0, NUM_BLOCKS);
  drawNext();
}

/**
 * Restarts the game.
 */
void restart() {
  dropped = 0;
  redCheck();
  buildHUD();

  // Gets starting index.
  index = random(0, NUM_BLOCKS);
  drawNext();
  holdIndex = -1;
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      leds[transform({(WIDTH - 1) * SIZE + 1 + x, (HEIGHT - 1) * SIZE + 1 + y})].setRGB(0, 0, 0);
    }
  }
}

/**
 * Takes a step down in the tetris game and determinds what happens
 * if the step hits another block.
 */
void step() {
  // Info to help find stats about the block.
  struct Position pos = blocks[index]->getPos();
  uint8_t size = blocks[index]->getSize();
  bool move = blocks[index]->setPos({ pos.x, pos.y + 1 });
  bool* shape = blocks[index]->getShape();
  if (move) blocks[index]->draw();
  else {
    // Clears all completed rows.
    for (int8_t y = size - 1; y >= 0; y--) {
      if (pos.y + y < (HEIGHT - 1) * SIZE) {
        bool clear = true;
        for (uint8_t x = 0; x < WIDTH * SIZE; x++) {
          if (leds[transform({x, pos.y + y})].getAverageLight() == 0) {
            clear = false;
          }
        }

        if (clear) {
          for (uint8_t x = 0; x < WIDTH * SIZE; x++) {
            leds[transform({x, pos.y + y})].setRGB(0, 0, 0);
          } 
        }
      }
    }
    
    // Updates screen to remove cleared rows.
    int8_t blackRow = min(pos.y + size - 1, (HEIGHT - 1) * SIZE - 1);
    int8_t colorRow = blackRow - 1;
    bool black = false;
    bool color = false;
    while (colorRow >= 0) {
      bool hunt = true;
      if (black && color) {
        // Brings the colored row to the black row and looks for next colored row.
        for (uint8_t x = 0; x < WIDTH * SIZE; x++) {
          leds[transform({x, blackRow})] = leds[transform({x, colorRow})];
          leds[transform({x, colorRow})].setRGB(0, 0, 0);
        }

        blackRow--;
        colorRow--;
        color = false;
      } else if (black) {
        // Finds the first colored row.
        hunt = false;
        for (uint8_t x = 0; x < WIDTH * SIZE && !hunt; x++) {
          if (leds[transform({x, colorRow})].getAverageLight() != 0) {
            hunt = true;
          }
        }
        color = hunt;
        if (!color) colorRow--;
      } else {
        // Finds the first black row.
        for (uint8_t x = 0; x < WIDTH * SIZE && hunt; x++) {
          if (leds[transform({x, blackRow})].getAverageLight() != 0) {
            blackRow--;
            colorRow--;
            hunt = false;
          }
        }
        black = hunt;
      }
    }

    // Checks if they lost.
    bool lost = false;
    for (uint8_t y = 0; y < size && !lost; y++) {
      for (uint8_t x = 0; x < size && !lost; x++) {
        lost = shape[y * size + x] && pos.y + y < 0;
      }
    }

    if (lost) {
      restart();
    } else {
      // Resets the blocks.
      blocks[index]->reset();

      // Gets the next index.
      index = nextIndex;
      drawNext();
      dropped += 1;
    }
  }
}

/**
 *  Holds onto the next item.
 */
void hold() {
  if (holdIndex < 0) {
    // Gets current pos.
    struct Position pos = blocks[index]->getPos();
    drawHold();

    // Sets the index in right spot.
    index = nextIndex;
    blocks[index]->setPos(pos);
    blocks[index]->draw();
    drawNext();
  } else {
    // Draws the index in hold index.
    int8_t temp = holdIndex;
    int8_t tempRot = holdRot;
    struct Position pos = blocks[index]->getPos();
    drawHold();

    // Updates the current index.
    index = temp;
    if (blocks[index]->setPos(pos)) {
      while (blocks[index]->getRotation() != tempRot) {
        blocks[index]->rotate(tempRot > 0);
      }
      blocks[index]->draw();
    } else {
      // Draws the index in hold index. If failed.
      int8_t temp = holdIndex;
      int8_t tempRot = holdRot;
      drawHold();

      index = temp;
      blocks[index]->setPos(pos);
      while (blocks[index]->getRotation() != tempRot) {
        blocks[index]->rotate(tempRot > 0);
      }
      blocks[index]->draw();
    }
  }
}

/**
 * Drops the item all the way down.
 */
void drop() {
  uint8_t lastDropped = dropped;
  while (lastDropped == dropped) {
    step();
  }
}

/**
 * Manages user input.
 */
void manageInput() {
  if (
    sCallbackData.justWritten &&
    sCallbackData.Flags != IRDATA_FLAGS_IS_REPEAT
  ) {
    sCallbackData.justWritten = false;
    bool draw = false;
    if (sCallbackData.Command == 0x7) draw = blocks[index]->setPos({blocks[index]->getPos().x - 1, blocks[index]->getPos().y});
    else if (sCallbackData.Command == 0x9) draw = blocks[index]->setPos({blocks[index]->getPos().x + 1, blocks[index]->getPos().y});
    else if (sCallbackData.Command == 0x15) drop();
    else if (sCallbackData.Command == 0x44) draw = blocks[index]->rotate(false);
    else if (sCallbackData.Command == 0x43) draw = blocks[index]->rotate(true);
    else if (sCallbackData.Command == 0x40) hold();
    if(draw) blocks[index]->draw();
    FastLED.show();
  }
}

/**
 * The game loop of the arduino.
 */
void loop() {
  manageInput();
  if (time + 2550 - dropped * 10 < millis()) {
    // Steps and updates time.
    step();
    FastLED.show();
    time = millis();
  }
}

/**
 * Manages events from the IR receiver.
 */
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags) {
  sCallbackData.Address = aAddress;
  sCallbackData.Command = aCommand;
  sCallbackData.Flags = aFlags;
  sCallbackData.justWritten = true;
}