#ifndef _STORAGE_h
#define _STORAGE_h

//#include <Arduino.h>
//#include <EEPROM.h>

//#include "Setup.h"

#include "stdlib.h"
#include "inttypes.h"

struct Data {
	uint32_t value0;
	uint32_t value1;
};

template<class T>
class EEPROMStorage
{
	/*
	|xxxx|xxxx|xxxx|xxxx|crc|0|0|0|
	*/

public: 
	EEPROMStorage(const uint8_t _blocks, const uint8_t _start_addr = 0);
	void add(const T &element);
	bool get(T &element);
	bool get_block(const uint8_t block, T &element);

	uint8_t crc_8( const unsigned char *input_str, size_t num_bytes ) ;

private:
	uint8_t start_addr;
	uint8_t activeBlock;
	uint8_t blocks;

	uint8_t elementSize;

	uint16_t flag_shift;
};


#endif

