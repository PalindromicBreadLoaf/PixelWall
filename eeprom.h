#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One byte per game. Keep addresses unique.
#define EEPROM_ADDR_SI_LEVEL      0   // Space Invaders: highest level reached
#define EEPROM_ADDR_STACKER_BEST  1   // Stacker: best score

// Blank EEPROM reads as 0xFF.
uint8_t eeprom_read(uint8_t addr);

// Write new data once the game is over.
void eeprom_write(uint8_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif
