#include "Setup.h"

#include <avr/pgmspace.h>
#include <USIWire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "counter.h"
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>  


// Для логирования раскомментируйте LOG_LEVEL_DEBUG в Setup.h

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif


#define BUTTON_PIN  4          //Вход 1, Blynk: V0
#define BUTTON2_PIN 3          //Вход 2, Blynk: V1

#define DEVICE_ID   4   	   // Модель устройства

#define ESP_POWER_PIN    1     // Номер пина, которым будим ESP8266. 
#define SETUP_BUTTON_PIN 2     // SCL Пин с кнопкой SETUP

#define LONG_PRESS 3000        // Долгое нажатие включает настройку

static unsigned long wake_every = WAKE_EVERY_MIN;

// Счетчики импульсов
static Counter counter(BUTTON_PIN);
static Counter counter2(BUTTON2_PIN);

static ESPPowerButton esp(ESP_POWER_PIN, SETUP_BUTTON_PIN);

// Данные
struct Header info = {DEVICE_ID, 0, 0, {0, 0} };

//100к * 20 = 2 млн * 10 л / 2 счетчика = 10 000 000 л или 10 000 м3
static EEPROMStorage<Data> storage(20); // 8 byte * 20 + crc * 20

SlaveI2C slaveI2C;

volatile int wdt_count; // таймер может быть < 0 ?

/* Вектор прерываний сторожевого таймера watchdog */
ISR( WDT_vect ) { 
	wdt_count--;
}  

/* Подготовка сторожевого таймера watchdog */
void resetWatchdog() {
	
	MCUSR = 0; // очищаем все флаги прерываний

	WDTCR = bit( WDCE ) | bit( WDE ); // allow changes, disable reset, clear existing interrupt

#ifdef TEST_WATERIUS
	WDTCR = bit( WDIE ) | bit( WDP0 );      // 32 ms
	#define ONE_MINUTE 20                   // ускоримся для теста
#else

	// настраиваем период
	//WDTCR = bit( WDIE );                  // 16 ms

	//WDTCR = bit( WDIE ) | bit( WDP0 ) | bit( WDP1 );     // 128 ms
	//#define ONE_MINUTE 480

	WDTCR = bit( WDIE ) | bit( WDP2 );                 // 250 ms
	#define ONE_MINUTE 240

	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // 500 ms
	//#define ONE_MINUTE 120
#endif

														
	wdt_reset(); // pat the dog
} 

inline void counting() {

	if (counter.check_close(info.data.value0)) {
		storage.add(info.data);
	}
	#ifndef DEBUG
		if (counter2.check_close(info.data.value1)) {
			storage.add(info.data);
		}
	#endif
}

void setup() {

	info.service = MCUSR; //причина перезагрузки
	storage.get(info.data);

	noInterrupts();
	ACSR |= bit( ACD ); //выключаем компаратор
	interrupts();
	resetWatchdog(); 
	adc_disable(); //выключаем ADC

#ifdef DEBUG
	DEBUG_CONNECT(9600); 
	LOG_DEBUG(F("==== START ===="));
	LOG_DEBUG(F("MCUSR"));
	LOG_DEBUG(info.service);
	LOG_INFO(F("Data:"));
	LOG_INFO(info.data.value0);
	LOG_INFO(info.data.value1);
#endif
}

void loop() {
	
	power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

	set_sleep_mode( SLEEP_MODE_PWR_DOWN );

	resetWatchdog(); 

	/*
		Если сон 250мс, то 1 минута через 240 раз
	*/
	for (unsigned int i = 0; i < ONE_MINUTE && !esp.pressed; ++i)  {

		wdt_count = wake_every; 
		while ( wdt_count > 0 ) {

			noInterrupts();
			
			counting();

			if (esp.sleep_and_pressed()) { //Пользователь нажал кнопку
				interrupts();
				break;
			} else 	{
				interrupts();
				sleep_mode();
			}
		}
	}
		
	wdt_disable();        // disable watchdog
	MCUSR = 0;
	power_all_enable();   // power everything back on

	info.voltage = readVcc();   // заранее запишем текущее напряжение
	storage.get(info.data);
	
	DEBUG_CONNECT(9600);
	LOG_INFO(F("Data:"));
	LOG_INFO(info.data.value0);
	LOG_INFO(info.data.value1);

	// Пользователь нажал кнопку SETUP
	// ждем когда пользователь отпустит кнопку 
	// т.к. иначе ESP запустится в режиме программирования
	if (esp.wait_button_release() > LONG_PRESS)
	{
		LOG_DEBUG(F("SETUP pressed"));
		wake_every = WAKE_AFTER_SETUP_MIN;

		slaveI2C.begin(SETUP_MODE);	
		esp.power(true);
		LOG_DEBUG(F("ESP turn on for SETUP"));
		
		while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(SETUP_TIME_MSEC)) {
			delayMicroseconds(65000);

			if (esp.wait_button_release() > LONG_PRESS) {
				break; // принудительно выключаем
			}
		}

	}
	else {
		LOG_DEBUG(F("wake up for transmitting"));
		wake_every = WAKE_EVERY_MIN;

		// Передаем показания
		slaveI2C.begin(TRANSMIT_MODE);
		esp.power(true);
		LOG_DEBUG(F("ESP turn on for TRANSMITTING"));

		//передаем данные в ESP
		while (!slaveI2C.masterGoingToSleep() 
			&& !esp.elapsed(WAIT_ESP_MSEC)) { 
			
			counting();
			delayMicroseconds(65000);
		}
	}

	esp.power(false);
	slaveI2C.end();			// выключаем i2c slave.

	LOG_DEBUG(F("ESP turn off. Wake up (min): "));
	LOG_DEBUG(wake_every);

	info.service = MCUSR;

	if (!slaveI2C.masterGoingToSleep()) {

		LOG_ERROR(F("ESP wake up fail"));
	}
}
