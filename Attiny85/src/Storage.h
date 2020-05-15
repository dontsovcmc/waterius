#ifndef _STORAGE_h
#define _STORAGE_h

#include <Arduino.h>
#include <EEPROM.h>

#include "Setup.h"

template<class T>
class EEPROMStorage
{
	/*
	Кольцевой буфер в памяти для хранения 1го значения. Чтобы превысить максимальное
	число записи в EEPROM (100 000). 

	blocks - кол-во копий значений.

	После блоков следует массив флагов для обозначения текущего блока буфера.
	Флаг - crc8 соответствующего блока. 

	При перезагрузке микроконтроллера текущий блок должен восстановиться.

	|xxxx|xxxx|xxxx|xxxx|crc|0|0|0|
	*/

public: 
	explicit EEPROMStorage(const uint8_t _blocks, const uint8_t _start_addr = 0);
	void add(const T &element);
	bool get(T &element);
	bool get_block(const uint8_t block, T &element);
	
	uint16_t size();	
private:
	uint8_t crc_8( const unsigned char *input_str, size_t num_bytes ) ;

	uint8_t start_addr;
	uint8_t activeBlock;
	uint8_t blocks;

	uint8_t elementSize;

	uint16_t flag_shift;
};


#endif
