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
		if (t > MARK * 2 || t == 0)  //мусор в EEPROM, *2 вдруг ресет во время записи?
			activeBlock = 0;
	}

	template< typename T >
	void add(const T &element)
	{	
		uint8_t prev = activeBlock;
		activeBlock = (activeBlock < blocks) ? activeBlock+1 : 0; 
		
		EEPROM.put(activeBlock * elementSize, element);
		
	}

private:
	uint8_t activeBlock;
	uint8_t blocks;

	uint8_t elementSize;

	uint16_t flag_shift;
};


class Storage
{
// protected:
	 byte ramStorage[STORAGE_SIZE];
	 uint16_t currentByte;
	 uint16_t addrPtr;
	 uint8_t elementSize;
	 
 public:
	 Storage( const uint8_t elementSize );
	 void addElement( const void * element );
	 void clear();

	 void gotoFirstByte();
	 byte getNextByte();
	 uint16_t getStoredByteCount();
	 uint8_t getElementSize();
};


#endif

