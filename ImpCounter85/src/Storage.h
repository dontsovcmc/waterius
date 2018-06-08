#ifndef _STORAGE_h
#define _STORAGE_h

#include <Arduino.h>
#include <EEPROM.h>

#include "Setup.h"

#define START_ADDR 0

template<class T>
class EEPROMStorage
{
	/*
	|xxxx|xxxx|xxxx|xxxx|x|0|0|0|
	*/
#define MARK 1

public: 
	EEPROMStorage(const uint8_t _blocks);
	void add(const T &element);
	bool get(T &element);

	void clear(uint8_t block_index);

	uint8_t crc_8( const unsigned char *input_str, size_t num_bytes ) ;

private:
	uint8_t activeBlock;
	uint8_t blocks;

	uint8_t elementSize;

	uint16_t flag_shift;
};


#endif

