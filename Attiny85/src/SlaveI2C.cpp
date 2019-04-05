#include "SlaveI2C.h"
#include <Arduino.h>
#include "Storage.h"
#include <USIWire.h>
#include "Setup.h"

extern struct Header info;
extern struct CounterState counter_states;

/* Static declaration */
uint8_t SlaveI2C::txBufferPos = 0;
uint8_t SlaveI2C::txBuffer[TX_BUFFER_SIZE];
bool SlaveI2C::masterSentSleep = false;
uint8_t SlaveI2C::lastCommand;
uint8_t SlaveI2C::setup_mode = TRANSMIT_MODE;


void SlaveI2C::begin(const uint8_t mode) {
	setup_mode = mode;
	Wire.begin( I2C_SLAVE_ADDRESS );
	Wire.onReceive( receiveEvent );
	Wire.onRequest( requestEvent );
	masterSentSleep = false;
	newCommand();
}

void SlaveI2C::end() {
	Wire.end();
}

void SlaveI2C::requestEvent() {
	Wire.write( txBuffer[txBufferPos] );
	if ( txBufferPos+1 < TX_BUFFER_SIZE ) txBufferPos++; // Avoid buffer overrun if master misbehaves
}

void SlaveI2C::newCommand() {
	memset( txBuffer, 0xAA, TX_BUFFER_SIZE );	// Zero the tx buffer (with 0xAA so master has a chance to see he is stupid)
	txBufferPos = 0;				// The next read from master starts from begining of buffer
	lastCommand = 0;				// No previous command was received
}

/* Depending on the received command from master, set up the content of the txbuffer so he can get his data */
void SlaveI2C::receiveEvent( int howMany ) {
	uint8_t command = Wire.read(); // Get instructions from master

	newCommand();
	switch ( command ) {
		case 'B':  // данные
			memcpy( txBuffer, &info, sizeof(info));
			break;
		case 'Z':  // Готовы ко сну
			masterSentSleep = true;
			break;
		case 'M':  // Разбудили ESP для настройки или передачи данных?
			txBuffer[0] = setup_mode;
			break;
	}
	lastCommand = command;
}

bool SlaveI2C::masterGoingToSleep() {
	return masterSentSleep;
}
