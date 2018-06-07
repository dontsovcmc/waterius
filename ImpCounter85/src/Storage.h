#ifndef _STORAGE_h
#define _STORAGE_h

#include <Arduino.h>
#include <EEPROM.h>

#include "Setup.h"

class EEPROMStorage
{
	/*
	|xxxx|xxxx|xxxx|xxxx|x|0|0|0|
	*/
#define MARK 1

public: 
	template< typename T >
	EEPROMStorage(const T &element, const uint8_t _blocks)
		: activeBlock(0)
		, blocks(blocks)
	{
		elementSize = sizeof(T);
		flag_shift = elementSize * blocks;
		//тут как то надо определить не испорчена ли память и если нет, продолжить работу
		int t = 0;
		for (int i = 0; i < blocks; i++)
		{
			t += EEPROM.read(flag_shift + i);
			if (t > 0)
				activeBlock = i;
		}
		if (t > MARK * 2 || t == 0)  //мусор в EEPROM, (х2 вдруг ресет во время записи?)
		{
			activeBlock = 0;
			for (int i = 0; i < flag_shift+blocks; i++)
				EEPROM.write(i, 0);
		}
	}

	template< typename T >
	void add(const T &element)
	{	
		uint8_t prev = activeBlock;
		activeBlock = (activeBlock < blocks) ? activeBlock+1 : 0; 
		
		EEPROM.put(activeBlock * elementSize, element);
		EEPROM.write(flag_shift + activeBlock, MARK);
		EEPROM.write(flag_shift + prev, 0);

		clear(prev);
	}

	void clear(uint8_t block_index)
	{
		for (int i=0; i < elementSize; i++)
			EEPROM.write(block_index * elementSize + i, 0);
	}

private:
	uint8_t activeBlock;
	uint8_t blocks;

	uint8_t elementSize;

	uint16_t flag_shift;
};


#endif

