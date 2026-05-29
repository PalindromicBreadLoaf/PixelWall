#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Persistent-storage address map — one byte per game.  Keep these unique;
   allocate a new address here rather than hard-coding a literal in a game. */
#define EEPROM_ADDR_SI_LEVEL      0   /* Space Invaders: highest level reached */
#define EEPROM_ADDR_STACKER_BEST  1   /* Stacker: best score (rows locked)     */

/* Read one byte from persistent storage. Returns 0xFF if the address has never
   been written (matches the blank-EEPROM state on a fresh Arduino). */
uint8_t eeprom_read(uint8_t addr);

/* Write one byte to persistent storage.  Writes are skipped if the stored
   value is already equal (Arduino EEPROM.update() semantics). */
void eeprom_write(uint8_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_H_ */
