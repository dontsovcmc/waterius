
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
static Counter counter, counter2;

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
	WAKE_MASTER_EVERY_MIN, //период передачи данных на сервер, мин  (требуется ручная калибровка внутреннего генератора)
	MEASUREMENT_EVERY_MIN, //период измерения данных в буфер, мин
	0, //напряжение питания, мВ (требуется ручная калибровка)
	0 //причина перезагрузки
};

Storage storage( sizeof( SensorData ) );
SlaveI2C slaveI2C;

void setup() 
{
	info.service = MCUSR; //причина перезагрузки

	counter.pin = BUTTON_PIN;
	counter2.pin = BUTTON2_PIN;
	
	DEBUG_CONNECT(9600);
  	LOG_DEBUG(F("==== START ===="));

	resetWatchdog(); 
	adc_disable(); //выключаем ADC

	//Проверим, что входы считают или 2 мин задержка.
	DEBUG_FLUSH();
	gotoDeepSleep(1, &counter, &counter2);
	DEBUG_CONNECT(9600);
	LOG_DEBUG(F("Counters ok"));
	LOG_DEBUG(counter.i); 
	LOG_DEBUG(counter2.i);

	minSleeping = WAKE_MASTER_EVERY_MIN;
	state = MEASURING;
}

void loop() 
{
	switch ( state ) {
		case SLEEP:
			LOG_DEBUG(F("LOOP (SLEEP)"));
			slaveI2C.end();			// выключаем i2c slave. 
			DEBUG_FLUSH();
			gotoDeepSleep( MEASUREMENT_EVERY_MIN, &counter, &counter2);	// Глубокий сон X минут
			DEBUG_CONNECT(9600);
			minSleeping += MEASUREMENT_EVERY_MIN;
			state = MEASURING;

			LOG_DEBUG(F("COUNTERS:")); 
			LOG_DEBUG(counter.i); 
			LOG_DEBUG(counter2.i);
			break;

		case MEASURING:
			LOG_DEBUG(F("LOOP (MEASURING)"));
			SensorData sensorData;
			sensorData.counter = counter.i;
			sensorData.counter2 = counter2.i;
			storage.addElement( &sensorData );

			state = SLEEP;			// Сохранили текущие значения
			if ( minSleeping >= WAKE_MASTER_EVERY_MIN ) 
				state = MASTER_WAKE; // пришло время будить ESP
			break;

		case MASTER_WAKE:
			LOG_DEBUG(F("LOOP (MASTER_WAKE)"));
			info.vcc = readVcc();   // заранее запишем текущее напряжение
			slaveI2C.begin();		// Включаем i2c slave
			wakeESP();              // импульс на reset ESP
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
			}

			if (millis() - masterWokenUpAt > GIVEUP_ON_MASTER_AFTER * 1000UL) 
			{
				LOG_ERROR(F("ESP wake up fail"));
				minSleeping = 0;
				state = SLEEP;
			}
			
			break;
	}
}