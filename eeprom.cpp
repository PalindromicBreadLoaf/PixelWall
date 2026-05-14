#include "eeprom.h"
#include <EEPROM.h>

uint8_t eeprom_read(uint8_t addr) {
    return EEPROM.read(addr);
}

void eeprom_write(uint8_t addr, uint8_t val) {
    EEPROM.update(addr, val);
}
