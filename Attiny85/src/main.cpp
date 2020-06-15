#include "Setup.h"

#include <avr/pgmspace.h>
#include <Wire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "counter.h"
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>  

// Для логирования раскомментируйте LOG_ON в Setup.h
#ifdef LOG_ON
	#ifdef WATERIUS_2C
		TinyDebugSerial mySerial;
	#endif
#endif


#define FIRMWARE_VER 13    // Версия прошивки. Передается в ESP и на сервер в данных.
  
/*
Версии прошивок 

13 - 2020.06.09 - dontsovcmc
	1. изменил формулу crc

12 - 2020.05.15 - dontsovcmc
	1. Добавил команду T для переключения режима пробуждения
	2. Добавил отправку аналогового уровня замыкания входа в ЕСП
	3. Исправил инициализацию входов. Кажется после перезагрузки +1 импульс
	4. Добавил crc при отправке данных

11 - 2019.10.20 - dontsovcmc
    1. Обновил алгоритм подсчёта импульсов.
	   Теперь импульс: 1 раз замыкание + 3 раза разомкнуто. Период 250мс +- 10%.
	
10 - 2019.09.16 - dontsovcmc 
    1. Замеряем питание пока общаемся с ESP
	2. Время настройки 10 минут.

9 - 2019.05.04 - dontsovcmc
    1. USIWire заменен на Wire

8 - 2019.04.05 - dontsovcmc
    1. Добавил поддержку НАМУР. Теперь чтение состояния analogRead
	2. Добавил состояние входов.

7 - 2019.03.01 - dontsovcmc
	1. Обновил фреймворк до Platformio Atmel AVR 1.12.5
	2. Время аварийного отключения ESP 120сек. 
	   Даже при отсутствии связи ESP раньше в таймауты уйдет и пришлет "спим".
	
*/
     

// Счетчики импульсов

#ifdef WATERIUS_2C
// Waterius Classic: https://github.com/dontsovcmc/waterius
//
//                                +-\/-+
//       RESET   (D  5/A0)  PB5  1|    |8  VCC
//  *Counter1*   (D  3/A3)  PB3  2|    |7  PB2  (D  2/ A1)         SCL   *Button*
//  *Counter0*   (D  4/A2)  PB4  3|    |6  PB1  (D  1)      MISO         *Power ESP*
//                          GND  4|    |5  PB0  (D  0)      MOSI   SDA   
//                                +----+
//
// https://github.com/SpenceKonde/ATTinyCore/blob/master/avr/extras/ATtiny_x5.md

static CounterB counter0(4, 2);  // Вход 1, Blynk: V0, горячая вода PB4 ADC2
static CounterB counter1(3, 3); // Вход 2, Blynk: V1, холодная вода (или лог) PB3 ADC3

static ButtonB button(2);	   // PB2 кнопка (на линии SCL)
                               // Долгое нажатие: ESP включает точку доступа с веб сервером для настройки
							   // Короткое: ESP передает показания
static ESPPowerPin esp(1);  // Питание на ESP 

// Данные
struct Header info = {FIRMWARE_VER, 0, 0, 0, WATERIUS_2C, 
					   {CounterState_e::CLOSE, CounterState_e::CLOSE},
				       {0, 0},
					   0, 0,
					   0, 0
					 };
#endif

#ifdef WATERIUS_4C2W

#define WDTCR WDTCSR
// Waterius 4C2W: https://github.com/badenbaden/Waterius-Attiny84-ESP12F
//
//                                 +-\/-+
//                           VCC  1|    |14  GND
//  *Power ESP*   (D  0)     PB0  2|    |13  PA0  (D  10/A0)        *Counter0* 
//  *Button*      (D  1)     PB1  3|    |12  PA1  (D  9/ A1)        *Counter1* 
//       RESET    (D 11)     PB3  4|    |11  PA2  (D  8/ A2)        *Counter2* 
//  *Alarm*       (D  2)     PB2  5|    |10  PA3  (D  7/ A3)        *Counter3* 
//  *WaterLeak2*  (D  3/A7)  PA7  6|    |9   PA4  (D  6/ A4)   SCK  SCL
//  SDA  MOSI     (D  4/A6)  PA6  7|    |8   PA5  (D  5/ A5)   MISO *WaterLeak2*
//                                 +----+
//
// https://github.com/SpenceKonde/ATTinyCore/blob/master/avr/extras/ATtiny_x4.md


static CounterA counter0(0, 0);
static CounterA counter1(1, 1);
static CounterA counter2(2, 2);
static CounterA counter3(3, 3);

static CounterA counter5(5, 5); //only for setup input pin
static CounterA counter7(7, 7); //only for setup input pin

static ButtonB button(1);
static ESPPowerPin esp(0);      // Питание на ESP

// Данные
struct Header info = {FIRMWARE_VER, 0, 0, 0, WATERIUS_4C2W, 
					   {CounterState_e::CLOSE, CounterState_e::CLOSE},
				       {0, 0},
					   0, 0,
					   0, 0
					 }; 
#endif




//Кольцевой буфер для хранения показаний на случай замены питания или перезагрузки
//Кольцовой нужен для того, чтобы превысить лимит записи памяти в 100 000 раз
//Записывается каждый импульс, поэтому для 10л/импульс срок службы памяти 10 000м3
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
	// Итак, пробуждаемся (проверяем входы) каждые 250 мс
	// 1 минута примерно равна 240 пробуждениям
	
#ifdef TEST_WATERIUS
	WDTCR = bit( WDIE ) | bit( WDP2 );      // bit( WDP0 )  32 ms
											// bit( WDP2 ) 250 ms
	#define ONE_MINUTE 240  
#else
	WDTCR = bit( WDIE ) | bit( WDP2 );     // 250 ms
	#define ONE_MINUTE 240
#endif
									
	wdt_reset(); // pat the dog
} 

// Проверяем входы на замыкание. 
// Замыкание засчитывается только при повторной проверке.
inline void counting() {

    power_adc_enable(); //т.к. мы обесточили всё а нам нужен компаратор
    adc_enable();       //после подачи питания на adc

	if (counter0.is_impuls()) {
		info.data.value0++;	  //нужен т.к. при пробуждении запрашиваем данные
		info.states.state0 = counter0.state;
		info.adc.adc0 = counter0.adc;
		storage.add(info.data);
	}
#ifndef LOG_ON
	if (counter1.is_impuls()) {
		info.data.value1++;
		info.states.state1 = counter1.state;
		info.adc.adc1 = counter1.adc;
		storage.add(info.data);
	}
#endif

#ifdef WATERIUS_4C2W
	if (counter2.is_impuls()) {
		info.data.value2++;
		info.states.state2 = counter2.state;
		info.adc.adc2 = counter2.adc;
		storage.add(info.data);
	}
	if (counter3.is_impuls()) {
		info.data.value3++;
		info.states.state3 = counter3.state;
		info.adc.adc3 = counter3.adc;
		storage.add(info.data);
	}
#endif

	adc_disable();
    power_adc_disable();

}

// Настройка. Вызывается однократно при запуске.
void setup() {

	info.service = MCUSR; //причина перезагрузки

	noInterrupts();
	ACSR |= bit( ACD ); //выключаем компаратор  TODO: не понятно, м.б. его надо повторно выключать в цикле 
	interrupts();
	resetWatchdog(); 
	//adc_disable(); //выключаем ADC. Теперь в цикле вкл/выкл, тут не нужен.

	if (storage.get(info.data)) { //не первая загрузка
		info.resets = EEPROM.read(storage.size());
		info.resets++;
		EEPROM.write(storage.size(), info.resets);
	} else {
		EEPROM.write(storage.size(), 0);
	}

	LOG_BEGIN(9600); 
	LOG(F("==== START ===="));
	LOG(F("MCUSR"));
	LOG(info.service);
	LOG(F("RESET"));
	LOG(info.resets);
	LOG(F("EEPROM used:"));
	LOG(storage.size() + 1);
	LOG(F("Data:"));
	LOG(info.data.value0);
	LOG(info.data.value1);
#ifdef WATERIUS_4C2W
	LOG(info.data.value2);
	LOG(info.data.value3);
#endif
}


// Главный цикл, повторящийся раз в сутки или при настройке вотериуса
void loop() {
	power_all_disable();  // Отключаем все лишнее: ADC, Timer 0 and 1, serial interface

	set_sleep_mode( SLEEP_MODE_PWR_DOWN );  // Режим сна

	resetWatchdog();  // Настраиваем служебный таймер (watchdog)

	// Цикл опроса входов
	// Выход по прошествию WAKE_EVERY_MIN минут или по нажатию кнопки
	for (unsigned int i = 0; i < ONE_MINUTE && !button.pressed(); ++i)  {
		wdt_count = WAKE_EVERY_MIN; 
		while ( wdt_count > 0 ) {
			noInterrupts();

			if (button.pressed()) { 
				interrupts();  // Пользователь нажал кнопку
				break;
			} else 	{
				counting(); //Опрос входов. Тут т.к. https://github.com/dontsovcmc/waterius/issues/76

				interrupts();
				sleep_mode();  // Спим (WDTCR)
			}
		}
	}
		
	wdt_disable();        // disable watchdog
	power_all_enable();   // power everything back on

	storage.get(info.data);     // Берем из хранилища текущие значения импульсов

	LOG_BEGIN(9600);
	LOG(F("Data:"));
	LOG(info.data.value0);
	LOG(info.data.value1);
#ifdef WATERIUS_4C2W
	LOG(info.data.value2);
	LOG(info.data.value3);
#endif
	// Если пользователь нажал кнопку SETUP, ждем когда отпустит 
	// иначе ESP запустится в режиме программирования (да-да кнопка на i2c и 2 пине ESP)
	// Если кнопка не нажата или нажата коротко - передаем показания 
	unsigned long wake_up_limit;
	if (button.wait_release() > LONG_PRESS_MSEC) {

		LOG(F("SETUP pressed"));
		slaveI2C.begin(SETUP_MODE);	
		wake_up_limit = SETUP_TIME_MSEC; //10 мин при настройке
	} else {

		LOG(F("wake up for transmitting"));
		slaveI2C.begin(TRANSMIT_MODE);
		wake_up_limit = WAIT_ESP_MSEC; //15 секунд при передаче данных
	}

	esp.power(true);
	LOG(F("ESP turn on"));
	
	while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(wake_up_limit)) {

		info.voltage = readVcc();   // Текущее напряжение

		counting();
		delayMicroseconds(65000);

		if (button.wait_release() > LONG_PRESS_MSEC) {
			break; // принудительно выключаем
		}
	}

	esp.power(false);
	slaveI2C.end();			// выключаем i2c slave.

	if (!slaveI2C.masterGoingToSleep()) {
		LOG(F("ESP wake up fail"));
	} else {
		LOG(F("Sleep received"));
	}
}
