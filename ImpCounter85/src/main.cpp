
#include "Setup.h"

#include <avr/pgmspace.h>
#include <USIWire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "sleep_counter.h"
#include <avr/wdt.h>

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif

static enum State state = SLEEP;
static unsigned long ticks_working = 0;

//глобальный счетчик до 65535
static Counter counter(BUTTON_PIN);
static Counter counter2(BUTTON2_PIN);

static ESPPowerButton esp(ESP_POWER_PIN, SETUP_BUTTON_PIN);

struct Header info = {0, DEVICE_ID, WAKE_MASTER_EVERY_TICKS, 0, 0, 0};

Storage storage(sizeof(Data));
SlaveI2C slaveI2C;

void setup() 
{
	info.service = MCUSR; //причина перезагрузки
	resetWatchdog(); 
	adc_disable(); //выключаем ADC

	DEBUG_CONNECT(9600);
  	LOG_DEBUG(F("==== START ===="));

	//Проверим, что входы считают или 2 мин задержка.
	gotoDeepSleep(1, &counter, &counter2, &esp);
	if (esp.pressed)
	{
		state = SETUP;
	}
	else
	{
		ticks_working = WAKE_MASTER_EVERY_TICKS;
		state = MEASURING;
	}
	DEBUG_CONNECT(9600);
	LOG_DEBUG(F("Counters ok"));
	LOG_DEBUG(counter.i); 
	LOG_DEBUG(counter2.i);
}

void loop() 
{
	static Data data;
	unsigned long now;

	switch ( state ) {
		case SLEEP:
			LOG_DEBUG(F("LOOP (SLEEP)"));
			data.counter = counter.i;
			data.counter2 = counter2.i;

			now = millis();
			gotoDeepSleep( MEASUREMENT_EVERY_MIN, &counter, &counter2, &esp);	// Глубокий сон X минут
			ticks_working += millis() - now;

			if (esp.pressed)
			{
				state = SETUP;
			}
			else if (data.counter != counter.i || data.counter2 != counter2.i)
			{
				state = MEASURING;
			}
			else if ( ticks_working >= WAKE_MASTER_EVERY_TICKS ) 
			{
				state = MASTER_WAKE; // пришло время будить ESP
			}
			break;

		case SETUP:
			slaveI2C.begin(SETUP_MODE);		// Включаем i2c slave
			esp.power(true);

			DEBUG_CONNECT(9600);
			LOG_DEBUG(F("ESP turn on"));

			now = millis();
			while (millis() - now > SETUP_TIME_MSEC)
			{
				delay(100);
				//gotoDeepSleep( 1, &counter, &counter2, &esp);	// Глубокий сон X минут
			}
			esp.power(false);
			LOG_DEBUG(F("ESP turn off"));

			ticks_working = WAKE_MASTER_EVERY_TICKS;
			state = MEASURING;
			break;

		case MEASURING:
			LOG_DEBUG(F("LOOP (MEASURING)"));
			data.counter = counter.i;
			data.counter2 = counter2.i;
			data.ticks = millis();
			storage.addElement( &data );

			state = SLEEP;			// Сохранили текущие значения
			if ( ticks_working >= WAKE_MASTER_EVERY_TICKS ) 
				state = MASTER_WAKE; // пришло время будить ESP
			break;

		case MASTER_WAKE:
			LOG_DEBUG(F("LOOP (MASTER_WAKE)"));
			info.vcc = readVcc();   // заранее запишем текущее напряжение
			info.service = MCUSR;
			slaveI2C.begin(TRANSMIT_MODE);		// Включаем i2c slave
			esp.power(true);
			state = SENDING;
			break;

		case SENDING:
			if (slaveI2C.masterGoingToSleep()) 
			{
				if (slaveI2C.masterGotOurData()) 
				{
					LOG_DEBUG(F("ESP ok"));
					storage.clear();
				}
				else
				{
					LOG_ERROR(F("ESP send fail"));
				}
				ticks_working = 0;
				state = SLEEP;
				esp.power(false);
				slaveI2C.end();			// выключаем i2c slave.
			}

			if (millis() - esp.wake_up_timestamp > WAIT_ESP_MSEC) 
			{
				LOG_ERROR(F("ESP wake up fail"));
				ticks_working = 0;
				state = SLEEP;
				esp.power(false);
				slaveI2C.end();			// выключаем i2c slave.
			}
			
			break;
	}
}