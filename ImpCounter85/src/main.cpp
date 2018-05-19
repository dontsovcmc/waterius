
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

//глобальный счетчик до 65535
static Counter counter(BUTTON_PIN);
static Counter counter2(BUTTON2_PIN);

static ESPPowerButton esp(ESP_POWER_PIN, SETUP_BUTTON_PIN);

struct Header info = {0, DEVICE_ID, MEASUREMENT_EVERY_MIN, 0, 0, 0};

static Data data;

Storage storage(sizeof(Data));
SlaveI2C slaveI2C;

void setup() 
{
	info.service = MCUSR; //причина перезагрузки
	noInterrupts();
	ACSR |= bit( ACD ); //выключаем компаратор
	interrupts();
	resetWatchdog(); 
	adc_disable(); //выключаем ADC

	DEBUG_CONNECT(9600);
  	LOG_DEBUG(F("==== START ===="));

	data.timestamp = WAKE_MASTER_EVERY_MIN;
  	LOG_DEBUG(data.timestamp);
	//Проверим, что входы считают или 2 мин задержка.
	gotoDeepSleep(1, &counter, &counter2, &esp);
	if (esp.pressed)
	{
		state = SETUP;
	}
	else
	{
		state = MEASURING;
	}
	DEBUG_CONNECT(9600);
	LOG_DEBUG(F("Counters ok"));
	LOG_DEBUG(counter.i); 
	LOG_DEBUG(counter2.i);
}

void loop() 
{
	switch ( state ) {
		case SLEEP:
			LOG_DEBUG(F("LOOP (SLEEP)"));
			data.counter = counter.i;
			data.counter2 = counter2.i;
  			LOG_DEBUG(data.timestamp);

			gotoDeepSleep( MEASUREMENT_EVERY_MIN, &counter, &counter2, &esp);	// Глубокий сон X минут
			
			DEBUG_CONNECT(9600);

			data.timestamp += MEASUREMENT_EVERY_MIN;
			LOG_DEBUG(data.timestamp);

			if (esp.pressed)
			{
				state = SETUP;
			}
			else if (data.counter != counter.i || data.counter2 != counter2.i)
			{
				state = MEASURING;
			}
			else if ( data.timestamp >= WAKE_MASTER_EVERY_MIN ) 
			{
				state = MEASURING; // пришло время будить ESP
			}
			break;

		case SETUP:
			
			while(esp.is_pressed())
				;  //ждем когда пользователь отпустит кнопку

			slaveI2C.begin(SETUP_MODE);		// Включаем i2c slave
			esp.power(true);

			DEBUG_CONNECT(9600);
			LOG_DEBUG(F("ESP turn on"));
			
			//&!&! TODO
			while (!slaveI2C.masterGoingToSleep() && millis() - esp.wake_up_timestamp < SETUP_TIME_MSEC) 
			{
				delayMicroseconds(65000);
			}

			esp.power(false);
			LOG_DEBUG(F("ESP turn off"));

			data.timestamp = WAKE_MASTER_EVERY_MIN;
			state = MEASURING;
			break;

		case MEASURING:
			DEBUG_CONNECT(9600);
			LOG_DEBUG(F("LOOP (MEASURING)"));
			data.counter = counter.i;
			data.counter2 = counter2.i;

			storage.addElement( &data );

			//если память полная, нужно передавать
			state = SLEEP;			// Сохранили текущие значения
			if (storage.is_full() || data.timestamp >= WAKE_MASTER_EVERY_MIN ) 
			{
				state = MASTER_WAKE; // пришло время будить ESP
			}
			break;

		case MASTER_WAKE:
			LOG_DEBUG(F("LOOP (MASTER_WAKE)"));
			if (storage.is_full())
				LOG_DEBUG(F("full"));
			info.vcc = readVcc();   // заранее запишем текущее напряжение
			info.service = MCUSR;
			slaveI2C.begin(TRANSMIT_MODE);		// Включаем i2c slave
			esp.power(true);
			state = SENDING;
			LOG_DEBUG(F("SENDING"));
			break;

		case SENDING:
			if (slaveI2C.masterGoingToSleep()) 
			{	
				if (slaveI2C.masterGotOurData()) 
				{
					LOG_DEBUG(F("ESP ok"));
					storage.clear();
					data.timestamp = 0;
				}
				else
				{
					LOG_ERROR(F("ESP send fail"));

					storage.clear(); // чтобы не включаться каждый measure из-за full
				}
				state = SLEEP;
				esp.power(false);
				slaveI2C.end();			// выключаем i2c slave.
			}

			if (millis() - esp.wake_up_timestamp > WAIT_ESP_MSEC) 
			{
				if (slaveI2C.masterModeChecked())
					LOG_INFO(F("mode was checked"));
				LOG_ERROR(F("ESP wake up fail"));
				state = SLEEP;
				esp.power(false);
				slaveI2C.end();			// выключаем i2c slave.
				storage.clear(); // чтобы не включаться каждый measure из-за full
			}
			
			break;
	}
}