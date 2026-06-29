#include "../eeprom.h"
#include <string.h>

// 0xFF matches blank Arduino EEPROM.
static uint8_t buf[16];
static int     inited = 0;

static void ensure_init(void) {
    if (!inited) {
        memset(buf, 0xFF, sizeof(buf));
        inited = 1;
    }
}

uint8_t eeprom_read(uint8_t addr) {
    ensure_init();
    if (addr >= sizeof(buf)) return 0xFF;
    return buf[addr];
}

void eeprom_write(uint8_t addr, uint8_t val) {
    ensure_init();
    if (addr < sizeof(buf) && buf[addr] != val)
        buf[addr] = val;
}
