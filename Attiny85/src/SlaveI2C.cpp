
#include "Setup.h"
#include "SlaveI2C.h"
#include <Arduino.h>
#include "Storage.h"
#include <Wire.h>
#include "waterleak.h"

extern struct Header info;

extern WaterLeakA waterleak1;
extern WaterLeakA waterleak2;

/* Static declaration */
uint8_t SlaveI2C::txBufferPos = 0;
uint8_t SlaveI2C::txBuffer[TX_BUFFER_SIZE];
uint8_t SlaveI2C::setup_mode = TRANSMIT_MODE;
bool SlaveI2C::masterSentSleep = false;

bool SlaveI2C::alarm_sent = true;

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
	//отключение подтяжек, требуется, т.к. Wire.cpp теущей версии платформы не делает этого
	DDRA &= ~(1<<4) & ~(1<<6); //SCL SDA to input
    PORTA &= ~(1<<4) & ~(1<<6); //SCL SDA pull-up disable
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
	uint8_t command = Wire.read(); // Get instructions from master
	
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

		case 'A':  // Тревога была отправлена
			alarm_sent = true;
			break;
		case 'L':  // Данные датчиков протечки
			LeakHeader *data = (LeakHeader*)(&txBuffer[0]);
			data->adc1 = waterleak1.adc;
			data->adc2 = waterleak2.adc;
			data->state1 = waterleak1.state;
			data->state2 = waterleak2.state;
			data->crc = crc_8((unsigned char*)data, LEAK_HEADER_SIZE);
			break;
	}
}

bool SlaveI2C::masterGoingToSleep() {
	return masterSentSleep;
}
