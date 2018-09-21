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


#define BUTTON_PIN  4          //Вход 1, Blynk: V0, горячая вода
#define BUTTON2_PIN 3          //Вход 2, Blynk: V1, холодная вода (или лог)

#define DEVICE_ID   5   	   // Модель устройства

#define ESP_POWER_PIN    1     // Номер пина, которым будим ESP8266. 
#define SETUP_BUTTON_PIN 2     // SCL Пин с кнопкой SETUP

#define LONG_PRESS_MSEC  3000  // Долгое нажатие = Настройка
                               // Короткое = Передать показания


// Счетчики импульсов
static Counter counter(BUTTON_PIN);
static Counter counter2(BUTTON2_PIN);

// Класс для подачи питания на ESP и нажатия кнопки
static ESPPowerButton esp(ESP_POWER_PIN);

// Данные
struct Header info = {DEVICE_ID, 0, 0, {0, 0} };

//Кольцевой буфер для хранения показаний на случай замены питания или перезагрузки
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

	// настраиваем период сна и кол-во просыпаний за 1 минуту
#ifdef TEST_WATERIUS
	WDTCR = bit( WDIE ) | bit( WDP0 );      // 32 ms
	#define ONE_MINUTE 20  // ускоримся для теста
#else

	//WDTCR = bit( WDIE );                  // 16 ms
	//#define ONE_MINUTE ?

	WDTCR = bit( WDIE ) | bit( WDP0 ) | bit( WDP1 );     // 128 ms
	#define ONE_MINUTE 480

	//WDTCR = bit( WDIE ) | bit( WDP2 );                 // 250 ms
	//#define ONE_MINUTE 240

	//WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP0 );    // 500 ms
	//#define ONE_MINUTE 120
#endif
									
	wdt_reset(); // pat the dog
} 

// Проверяем входы на замыкание. 
// Замыкание засчитывается только при повторной проверке.
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

// Настройка. Вызывается однократно при запуске.
void setup() {

	info.service = MCUSR; //причина перезагрузки
	storage.get(info.data);

	noInterrupts();
	ACSR |= bit( ACD ); //выключаем компаратор
	interrupts();
	resetWatchdog(); 
	adc_disable(); //выключаем ADC

	pinMode(SETUP_BUTTON_PIN, INPUT); //кнопка на корпусе

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

// Проверка нажатия кнопки
bool button_pressed(){

	if (digitalRead(SETUP_BUTTON_PIN) == LOW)
	{	//защита от дребезга
		delayMicroseconds(20000);  //нельзя delay, т.к. power_off
		return digitalRead(SETUP_BUTTON_PIN) == LOW;
	}
	return false;
}

// Замеряем сколько времени нажата кнопка в мс
unsigned long wait_button_release(){

	unsigned long press_time = millis();
	while(button_pressed())
		;  
	return millis() - press_time;
}


// Главный цикл, повторящийся раз в сутки или при настройке вотериуса
void loop() {
	
	// Отключаем все лишнее: ADC, Timer 0 and 1, serial interface
	power_all_disable();  

	// Режим сна
	set_sleep_mode( SLEEP_MODE_PWR_DOWN );

	// Настраиваем служебный таймер (watchdog)
	resetWatchdog(); 

	// Цикл опроса входов
	// Выход по прошествию WAKE_EVERY_MIN минут или по нажатию кнопки
	for (unsigned int i = 0; i < ONE_MINUTE && !button_pressed(); ++i)  {
		wdt_count = WAKE_EVERY_MIN; 
		while ( wdt_count > 0 ) {
			noInterrupts();

			counting(); //Опрос входов

			if (button_pressed()) { 
				interrupts();  // Пользователь нажал кнопку
				break;
			} else 	{
				interrupts();
				sleep_mode();  // Спим (WDTCR)
			}
		}
	}
		
	wdt_disable();        // disable watchdog
	MCUSR = 0;            
	power_all_enable();   // power everything back on

	info.voltage = readVcc();   // Текущее напряжение
	storage.get(info.data);     // Берем из хранилища текущие значения импульсов
	
	DEBUG_CONNECT(9600);
	LOG_INFO(F("Data:"));
	LOG_INFO(info.data.value0);
	LOG_INFO(info.data.value1);

	// Если пользователь нажал кнопку SETUP, ждем когда отпустит 
	// иначе ESP запустится в режиме программирования (да-да кнопка на i2c и 2 пине ESP)
	// Если кнопка не нажата или нажата коротко - передаем показания 
	if (wait_button_release() > LONG_PRESS_MSEC) {
		LOG_DEBUG(F("SETUP pressed"));

		slaveI2C.begin(SETUP_MODE);	
		esp.power(true);
		LOG_DEBUG(F("ESP turn on for SETUP"));
		
		while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(SETUP_TIME_MSEC)) {
			delayMicroseconds(65000);
			if (wait_button_release() > LONG_PRESS_MSEC) {
				break; // принудительно выключаем
			}
		}
	}
	else {
		LOG_DEBUG(F("wake up for transmitting"));

		// Передаем показания
		slaveI2C.begin(TRANSMIT_MODE);
		esp.power(true);
		LOG_DEBUG(F("ESP turn on for TRANSMITTING"));

		//передаем данные в ESP
		while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(WAIT_ESP_MSEC)) { 
			counting();
			delayMicroseconds(65000);
		}
	}

	esp.power(false);
	slaveI2C.end();			// выключаем i2c slave.

	info.service = MCUSR;

	if (!slaveI2C.masterGoingToSleep()) {
		LOG_ERROR(F("ESP wake up fail"));
	}
}
