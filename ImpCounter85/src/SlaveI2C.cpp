#include "SlaveI2C.h"
#include <Arduino.h>
#include "Storage.h"
#include <USIWire.h>
#include "Setup.h"
#include <TinyDebugSerial.h>

extern Storage storage;
extern struct SlaveStats info;
#ifdef DEBUG
	extern TinyDebugSerial mySerial;
#endif

/* Static declaration */
uint8_t SlaveI2C::txBufferPos = 0;
byte SlaveI2C::txBuffer[TX_BUFFER_SIZE];
bool SlaveI2C::masterSentSleep = false;
bool SlaveI2C::masterAckOurData = false;
char SlaveI2C::lastCommand;


/* Set up I2C slave handle ISR's */
void SlaveI2C::begin() {
	Wire.begin( I2C_SLAVE_ADDRESS );
	Wire.onReceive( receiveEvent );
	Wire.onRequest( requestEvent );
	masterSentSleep = false;
	masterAckOurData = false;
	newCommand();
}


/* Finishes talking to the slave - this allows us to become a master when talking to sensors*/
void SlaveI2C::end() {
	Wire.end();
}

/* When master pulls a byte from us give him the current byte of the txbuffer and increase the position */
void SlaveI2C::requestEvent() {
	Wire.write( txBuffer[txBufferPos] );
	if ( txBufferPos+1 < TX_BUFFER_SIZE ) txBufferPos++; // Avoid buffer overrun if master misbehaves

	if ( lastCommand == 'D' ) { // If master is reading storage, we keep giving hime the next byte
		txBuffer[0] = storage.getNextByte();
		txBufferPos = 0;
	}
}

/* Makes txbuffer ready */
void SlaveI2C::newCommand() {
	memset( txBuffer, 0xAA, TX_BUFFER_SIZE );	// Zero the tx buffer (with 0xAA so master has a chance to see he is stupid)
	txBufferPos = 0;				// The next read from master starts from begining of buffer
	lastCommand = 0;				// No previous command was received
}

/* Depending on the received command from master, set up the content of the txbuffer so he can get his data */
void SlaveI2C::receiveEvent( int howMany ) {
	byte command = Wire.read(); // Get instructions from master

	DEBUG_PRINTLN("I2C: cmd " + command);

	newCommand();
	switch ( command ) {
		case 'B': // If we get the cmd 'B' he asks for the number of bytes in storage that he can expect.
			info.bytesReady = storage.getStoredByteCount();
			memcpy( txBuffer, &info, sizeof(info));
			break;
		case 'D': // If we get the cmd 'D' from master, read the next element number and give it to him
			storage.gotoFirstByte();
			txBuffer[0] = storage.getNextByte();
			break;
		case 'A': // Master acknowledge that data is passed on.
			masterAckOurData = true;
			break;
		case 'Z': // Our master is going to sleep.
			masterSentSleep = true;
			break;
	}
	lastCommand = command;
}

/* Returns true if master has sent a 'Z' command, indicating that he is going to sleep */
bool SlaveI2C::masterGoingToSleep() {
	return masterSentSleep;
}

/* Returns true if master has acknowledged all data sent to him */
bool SlaveI2C::masterGotOurData() {
	return masterAckOurData;
}