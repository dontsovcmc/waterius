#include "Storage.h"



/* Storage constructor. We point to the start of storage */
Storage::Storage( const uint8_t elementSize ) {
	this->elementSize = elementSize;
	clear();
}



/* Adds a provided data element to storage. 
   If storage is full, the oldest element is thrown away */
void Storage::addElement( const void * element ) {
	if ( addrPtr + elementSize >= STORAGE_SIZE ) { // Storage Full
		memcpy( ramStorage, ramStorage + elementSize, STORAGE_SIZE - elementSize ); // Make space for an extra element.
		addrPtr -= elementSize;
	}
	for ( uint8_t i=0; i < elementSize; i++ ) {
		byte toWrite = *( (byte*) element + i );
		ramStorage[addrPtr] = toWrite;
		addrPtr++;
	}
}



/* Resets the storage address pointer, so we can start writing in the beginning of the eeprom (or ram)
There is no need to zero it out because that wears out the EEPROM unnesecaryly*/
void Storage::clear() {
	addrPtr = 0;
}



/* Set the read pointer to the first byte of storage area */
void Storage::gotoFirstByte() {
	currentByte = 0;
}



/* Fetch one byte from datastorage. 
   It returns true if a byte is fetched, otherwise false (we reached the end of storage) */
byte Storage::getNextByte() {
	if ( currentByte < addrPtr ) {
		byte readByte = ramStorage[currentByte];

		currentByte++;
		return readByte;
	} else return 0;
}



/* Returns the number of bytes stored */
uint16_t Storage::getStoredByteCount() {
	return addrPtr;
}


uint8_t Storage::getElementSize() {
	return elementSize;
}