
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

//для логгирования раскомментируйте LOG_LEVEL_DEBUG в Setup.h

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif

#define BUTTON_PIN  4
#define BUTTON2_PIN 3

#define WAIT_ESP_MSEC   10000UL       // Сколько секунд ждем передачи данных в ESP
#define SETUP_TIME_MSEC 300000UL      // Сколько пользователь настраивает ESP

#define WAKE_EVERY_MIN                1U // 24U * 60U

#define DEVICE_ID 3                   // Модель устройства

#define ESP_POWER_PIN    1		 // Номер пина, которым будим ESP8266. 
#define SETUP_BUTTON_PIN 2       // SCL Пин с кнопкой SETUP


//глобальный счетчик до 65535
static Counter counter(BUTTON_PIN);
static Counter counter2(BUTTON2_PIN);

static ESPPowerButton esp(ESP_POWER_PIN, SETUP_BUTTON_PIN);

struct Header info = {0, DEVICE_ID, WAKE_EVERY_MIN, 0, 0, 0};

//одно измерение
static struct Data {
	unsigned short counter;	     
	unsigned short counter2;  
} data; 

//EEPROMStorage estorage(sizeof(Data), 5);
Storage storage(sizeof(Data));
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
	WDTCR = bit( WDIE ) | bit( WDP0 );    // set WDIE, and 32 ms
	//WDTCR = bit( WDIE ) | bit( WDP2 );    // set WDIE, and 0.25 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // set WDIE, and 0.5 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 ) | bit( WDP0 );    // set WDIE, and 2 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 


void setup() 
{
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
		wdt_count = WAKE_EVERY_MIN; 
		while ( wdt_count > 0 ) 
		{
			noInterrupts();

			if (counter.check_close()) { //eeprom safe
				data.counter = counter.i;
			    storage.addElement( &data );
			}
			#ifndef DEBUG
				if (counter2.check_close()) { //eeprom safe
					data.counter2 = counter2.i;
					storage.addElement( &data ); 
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


	info.vcc = readVcc();   // заранее запишем текущее напряжение
	info.service = MCUSR;

	DEBUG_CONNECT(9600);

	// Пользователь нажал кнопку SETUP
	if (esp.pressed)
	{
		while(esp.is_pressed())
				;  //ждем когда пользователь отпустит кнопку т.к. иначе ESP запустится в режиме программирования

		slaveI2C.begin(SETUP_MODE);	

		esp.power(true);
		LOG_DEBUG(F("ESP turn on"));
		
		while (!slaveI2C.masterGoingToSleep() && millis() - esp.wake_up_timestamp < SETUP_TIME_MSEC) {
			delayMicroseconds(65000);
	        //НЕ надо тут импульсы считать, чтобы тут же прислать
			//надо сначала подключить
			//пролить 10 л
			//нажимать SETUP
		}

		esp.power(false);
		LOG_DEBUG(F("ESP turn off"));

		delayMicroseconds(65000);
	}
	
	// Передаем показания
	slaveI2C.begin(TRANSMIT_MODE);
	esp.power(true);

	while (!slaveI2C.masterGoingToSleep() && (millis() - esp.wake_up_timestamp > WAIT_ESP_MSEC)) {
		delay(1); //передаем данные в ESP
	}

	esp.power(false);
	slaveI2C.end();			// выключаем i2c slave.
	storage.clear();

#ifdef DEBUG
	if (slaveI2C.masterGotOurData()) {
		LOG_DEBUG(F("ESP ok"));
	}
	else {
		LOG_ERROR(F("ESP send fail"));
	}

	if (!slaveI2C.masterGoingToSleep()) {
		LOG_ERROR(F("ESP wake up fail"));
	}
#endif

}
