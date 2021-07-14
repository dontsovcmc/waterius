
#include "Setup.h"
#include "SlaveI2C.h"
#include <Arduino.h>
#include "Storage.h"
#include <Wire.h>

extern struct Header info;
extern uint32_t wakeup_period;

/* Static declaration */
uint8_t SlaveI2C::txBufferPos = 0;
uint8_t SlaveI2C::txBuffer[TX_BUFFER_SIZE];
uint8_t SlaveI2C::setup_mode = TRANSMIT_MODE;
bool SlaveI2C::masterSentSleep = false;

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
	if (txBufferPos+1 < TX_BUFFER_SIZE) txBufferPos++; // Avoid buffer overrun if master misbehaves
}

void SlaveI2C::newCommand() {
	memset(txBuffer, 0xAA, TX_BUFFER_SIZE);	// Zero the tx buffer (with 0xAA so master has a chance to see he is stupid)
	txBufferPos = 0;				// The next read from master starts from begining of buffer
}

/* Depending on the received command from master, set up the content of the txbuffer so he can get his data */

void SlaveI2C::receiveEvent(int howMany) {
	static uint8_t command;

	command = Wire.read(); // Get instructions from master

	newCommand();
	switch (command) {
		case 'B':  // данные
			info.crc = crc_8((unsigned char*)&info, HEADER_DATA_SIZE);
			memcpy(txBuffer, &info, TX_BUFFER_SIZE);
			break;
		case 'Z':  // Готовы ко сну
			masterSentSleep = true;
			break;
		case 'M':  // Разбудили ESP для настройки или передачи данных?
			txBuffer[0] = setup_mode;
			break;
		case 'T':  // Не используется. После настройки ESP перезагрузим, поэтому меняем режим на передачу данных
			setup_mode = TRANSMIT_MODE;
			break;
		case 'S': //ESP присылает новое значение периода пробуждения
			getWakeUpPeriod();
			break;

	}
}

void SlaveI2C::getWakeUpPeriod(){
	uint8_t data[2];

	data[0] = Wire.read();
	data[1] = Wire.read();
	uint8_t crc = Wire.read();

	uint16_t newPeriod = (data[0]<<8) | data[1];

	if ((crc == crc_8(data, 2)) && (newPeriod != 0)) { 
		wakeup_period = ONE_MINUTE * newPeriod;
	}
}

bool SlaveI2C::masterGoingToSleep() {
	return masterSentSleep;
}
