
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

//ИНСТАЛЯЦИЯ: 
//1. Добавляем в боте счетчик по id и паролю
//2. подключаем к счетчикам: Вход №1 - ГВС, Вход №2 - ХВС
//3. Проливаем 10 л каждой воды
//4. Нажимаем SETUP и настраиваем подключение
//5. После настройки должно прийти сообщение с ненулевыми показаниями

// Можно если нажали СЕТАП менять задержку на несколько минут. А после возвращать.

// Для логирования раскомментируйте LOG_LEVEL_DEBUG в Setup.h

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif

#define BUTTON_PIN  4
#define BUTTON2_PIN 3

#define DEVICE_ID 3                   // Модель устройства

#define ESP_POWER_PIN    1		 // Номер пина, которым будим ESP8266. 
#define SETUP_BUTTON_PIN 2       // SCL Пин с кнопкой SETUP

static unsigned long wake_every = WAKE_EVERY_MIN;

// Счетчики импульсов
static Counter counter(BUTTON_PIN);
static Counter counter2(BUTTON2_PIN);

static ESPPowerButton esp(ESP_POWER_PIN, SETUP_BUTTON_PIN);

// данные
struct Header info = {DEVICE_ID, 0, 0, {0, 0} };

//100к * 20 = 2 млн * 10 л / 2 = 10 000 000 л или 10 000 м3
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

	// настраиваем период
	//WDTCR = bit( WDIE );                  // 16 ms
	//WDTCR = bit( WDIE ) | bit( WDP0 );    // 32 ms
	WDTCR = bit( WDIE ) | bit( WDP2 );      // 250 ms
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // 500 ms
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // 1s
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 ) | bit( WDP0 );  // 2s 
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // 8s
														
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
	for (unsigned int i = 0; i < 240 && !esp.pressed; ++i)  {

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
	if (esp.pressed) {

		wake_every = WAKE_AFTER_SETUP_MIN;

		while(esp.is_pressed())
				;  //ждем когда пользователь отпустит кнопку т.к. иначе ESP запустится в режиме программирования

		slaveI2C.begin(SETUP_MODE);	
		esp.power(true);
		LOG_DEBUG(F("ESP turn on for SETUP"));
		
		while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(SETUP_TIME_MSEC)) {
			delayMicroseconds(65000);
		}

	}
	else {

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
