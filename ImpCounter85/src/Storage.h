#ifndef _STORAGE_h
#define _STORAGE_h

#include <Arduino.h>

#include "Setup.h"

//одно измерение
struct Data {
	unsigned short counter;	     
	unsigned short counter2;    
	unsigned short timestamp; 
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

