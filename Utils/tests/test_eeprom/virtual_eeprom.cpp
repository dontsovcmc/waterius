#include "virtual_eeprom.h"

uint8_t eeprom[512];

uint8_t eeprom_read_byte(uint8_t *idx) {
	return eeprom[(uint8_t)(void*)idx];
}
void eeprom_write_byte(uint8_t *idx, uint8_t val) {
	eeprom[(uint8_t)(void*)idx] = val;
}


