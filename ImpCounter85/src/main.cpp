
// Включение логгирования с TinySerial: 3 pin TX -> RX (TTL-USB 3.3 или 5в), 9600 8N1
// При логгировании не работает счетчик2 на 3-м пине.

// #define LOG_LEVEL_ERROR
// #define LOG_LEVEL_INFO
// #define LOG_LEVEL_DEBUG

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

//ИНСТАЛЛЯЦИЯ: 
//1. Добавляем в боте счетчик по id и паролю
//2. подключаем к счетчикам: Вход №1 - ГВС, Вход №2 - ХВС
//3. Проливаем 10 л каждой воды
//4. Нажимаем SETUP и настраиваем подключение
//5. После настройки должно прийти сообщение с ненулевыми показаниями

// Можно если нажали СЕТАП менять задержку на несколько минут. А после возвращать.

//для логирования раскомментируйте LOG_LEVEL_DEBUG в Setup.h

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif

#define BUTTON_PIN  4
#define BUTTON2_PIN 3

#define WAIT_ESP_MSEC   10000UL       // Сколько секунд ждем передачи данных в ESP
#define SETUP_TIME_MSEC 300000UL      // Сколько пользователь настраивает ESP

#define WAKE_EVERY_MIN                10U // 24U * 60U

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

//EEPROMStorage<Data> storage(15);

SlaveI2C slaveI2C;

volatile int wdt_count; // таймер может быть < 0 ?

/* Вектор прерываний сторожевого таймера watchdog */
ISR( WDT_vect ) { 
	wdt_count--;
}  

/* Подготовка сторожевого таймера watchdog */
void resetWatchdog() 
{
	MCUSR = 0; // очищаем все флаги прерываний

	WDTCR = bit( WDCE ) | bit( WDE ); // allow changes, disable reset, clear existing interrupt

	// настраиваем период
	//WDTCR = bit( WDIE );    // set WDIE, and 16 ms
	//WDTCR = bit( WDIE ) | bit( WDP0 );    // set WDIE, and 32 ms
	WDTCR = bit( WDIE ) | bit( WDP2 );    // set WDIE, and 0.25 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // set WDIE, and 0.5 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 ) | bit( WDP0 );    // set WDIE, and 2 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 


void setup() 
{
	info.data.value0 = 1000000;
	info.data.value1 = 1000000;
	info.service = MCUSR; //причина перезагрузки
	noInterrupts();
	ACSR |= bit( ACD ); //выключаем компаратор
	interrupts();
	resetWatchdog(); 
	adc_disable(); //выключаем ADC

	DEBUG_CONNECT(9600); LOG_DEBUG(F("==== START ===="));
}


void loop() 
{
	power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

	set_sleep_mode( SLEEP_MODE_PWR_DOWN );

	resetWatchdog(); 

	for (unsigned int i = 0; i < 240 && !esp.pressed; ++i)   //~1 min: watchdog 250ms: 60 * 4 = 240 раз
	{
		wdt_count = wake_every; 
		while ( wdt_count > 0 ) 
		{
			noInterrupts();

			if (counter.check_close(info.data.value0)) {
				;//storage.add(info.data);
			}
			#ifndef DEBUG
				if (counter2.check_close(info.data.value1)) {
					;//storage.add(info.data);
				}
			#endif

			if (esp.sleep_and_pressed()) //Пользователь нажал кнопку
			{
				interrupts();
				break;
			}
			else
			{
				interrupts();
				sleep_mode();
			}
		}
	}
		
	wdt_disable();        // disable watchdog
	MCUSR = 0;
	power_all_enable();   // power everything back on

	info.voltage = readVcc();   // заранее запишем текущее напряжение
	//storage.get(info.data);
	
	DEBUG_CONNECT(9600);

	wake_every = WAKE_EVERY_MIN;
	// Пользователь нажал кнопку SETUP
	if (esp.pressed)
	{
		wake_every = 1;

		while(esp.is_pressed())
				;  //ждем когда пользователь отпустит кнопку т.к. иначе ESP запустится в режиме программирования

		slaveI2C.begin(SETUP_MODE);	

		esp.power(true);
		LOG_DEBUG(F("ESP turn on"));
		
		while (!slaveI2C.masterGoingToSleep() && esp.elapsed(SETUP_TIME_MSEC)) {
			delayMicroseconds(65000);
		}

	}
	else {
		// Передаем показания
		slaveI2C.begin(TRANSMIT_MODE);
		esp.power(true);

		while (!slaveI2C.masterGoingToSleep() 
			&& !esp.elapsed(WAIT_ESP_MSEC)) { 
			; //передаем данные в ESP
		}

	}

	esp.power(false);
	LOG_DEBUG(F("ESP turn off"));
	slaveI2C.end();			// выключаем i2c slave.
	
	info.service = MCUSR;   // чтобы первое включение передалось

#ifdef DEBUG
	if (!slaveI2C.masterGoingToSleep()) {
		LOG_ERROR(F("ESP wake up fail"));
	}
#endif

}
