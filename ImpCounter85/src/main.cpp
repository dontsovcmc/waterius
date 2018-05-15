
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
static unsigned short minSleeping = 0;

static unsigned long masterWokenUpAt; //millis

//глобальный счетчик до 65535
static Counter counter(BUTTON_PIN);
#ifdef BUTTON2_PIN
	static Counter counter2(BUTTON2_PIN);
#else
	static Counter counter2(BUTTON_PIN);
#endif
static ESPPowerButton esp(ESP_EN_PIN);

//одно измерение
struct SensorData {
	unsigned short counter;	     
	unsigned short counter2;     
};

//Заголовок данных для ESP.
//размер кратен 2байтам (https://github.com/esp8266/Arduino/issues/1825)
struct SlaveStats info = {
	0, //байт прочитано
	DEVICE_ID, //модель устройства. пока = 1
	WAKE_MASTER_EVERY_TICKS, //период передачи данных на сервер, мин  (требуется ручная калибровка внутреннего генератора)
	MEASUREMENT_EVERY_MIN, //период измерения данных в буфер, мин
	0, //напряжение питания, мВ (требуется ручная калибровка)
	0 //причина перезагрузки
};

Storage storage( sizeof( SensorData ) );
SlaveI2C slaveI2C;

void setup() 
{
	info.service = MCUSR; //причина перезагрузки

	DEBUG_CONNECT(9600);
  	LOG_DEBUG(F("==== START ===="));

	resetWatchdog(); 
	adc_disable(); //выключаем ADC

	while (1)
	{
		esp.check();
		if (esp.pressed) //Пользователь нажал кнопку
		{
			esp.power(true);
		}
		else
		{
			esp.power(false);
		}
	}


	//Проверим, что входы считают или 2 мин задержка.
	gotoDeepSleep(1, &counter, &counter2, &esp);
	if (esp.pressed)
	{
		state = SETUP;
	}
	else
	{
		minSleeping = WAKE_MASTER_EVERY_TICKS;
		state = MEASURING;
	}
	DEBUG_CONNECT(9600);
	LOG_DEBUG(F("Counters ok"));
	LOG_DEBUG(counter.i); 
	LOG_DEBUG(counter2.i);
}

void loop() 
{
	static SensorData sensorData;
	unsigned long now;

	switch ( state ) {
		case SLEEP:
			LOG_DEBUG(F("LOOP (SLEEP)"));

			sensorData.counter = counter.i;
			sensorData.counter2 = counter2.i;

			now = millis();
			gotoDeepSleep( MEASUREMENT_EVERY_MIN, &counter, &counter2, &esp);	// Глубокий сон X минут
			minSleeping += millis() - now;

			if (esp.pressed)
			{
				state = SETUP;
			}
			else if (sensorData.counter != counter.i  ||
					 sensorData.counter2 != counter2.i)
			{
				state = MEASURING;

				DEBUG_CONNECT(9600);
				LOG_DEBUG(F("COUNTERS:"));
				LOG_DEBUG(counter.i);
				LOG_DEBUG(counter2.i);
			}
			else if ( minSleeping >= WAKE_MASTER_EVERY_TICKS ) 
			{
				state = MASTER_WAKE; // пришло время будить ESP
			}
			break;

		case SETUP:
			slaveI2C.begin(true);		// Включаем i2c slave
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

			minSleeping = WAKE_MASTER_EVERY_TICKS;
			state = MEASURING;
			break;

		case MEASURING:
			LOG_DEBUG(F("LOOP (MEASURING)"));
			SensorData sensorData;
			sensorData.counter = counter.i;
			sensorData.counter2 = counter2.i;
			//sensorData.timestamp = millis();
			storage.addElement( &sensorData );

			state = SLEEP;			// Сохранили текущие значения
			if ( minSleeping >= WAKE_MASTER_EVERY_TICKS ) 
				state = MASTER_WAKE; // пришло время будить ESP
			break;

		case MASTER_WAKE:
			LOG_DEBUG(F("LOOP (MASTER_WAKE)"));
			info.vcc = readVcc();   // заранее запишем текущее напряжение
			info.service = MCUSR;
			slaveI2C.begin(false);		// Включаем i2c slave
			esp.power(true);
			masterWokenUpAt = millis();  //запомнили время пробуждения
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
				minSleeping = 0;
				state = SLEEP;
				esp.power(false);
				slaveI2C.end();			// выключаем i2c slave.
			}

			if (millis() - masterWokenUpAt > WAIT_ESP_MSEC) 
			{
				LOG_ERROR(F("ESP wake up fail"));
				minSleeping = 0;
				state = SLEEP;
				esp.power(false);
				slaveI2C.end();			// выключаем i2c slave.
			}
			
			break;
	}
}