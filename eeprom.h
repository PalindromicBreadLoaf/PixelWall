#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
