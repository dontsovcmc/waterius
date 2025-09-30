#include "Setup.h"

#include <avr/pgmspace.h>
#include <Wire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "counter.h"
#include "button.h"
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>

// Для логирования раскомментируйте LOG_ON в Setup.h
#if defined(LOG_ON)
TinyDebugSerial mySerial;
#endif

/*
Версии прошивок

42 - 2025.09.30 - waterius-mini - dontsovcmc
    1. 250мс замыкание + 750мс размыкание = импульс
	2. ADC замыкания теперь 150 ~2кОм. Был 170 ~3.5кОм 
	5754 bytes, LOG_ON=7560 bytes

33 - 2025.06.16 - waterius-mini - dontsovcmc
	1. Добавлена команда продолжения бордствования ESP
	2. ИЗМЕНЕНА СТРУКТУРА ОТВЕТА. Совместимость с ESP от 1.2.0


======= 
41 - 2025.09.29 - dontsov
    1. 250мс замыкание + 750мс размыкание = импульс
	2. ADC замыкания теперь 150 ~2кОм. Был 170 ~3.5кОм 
	LOG_ON=6820 bytes

32 - 2023.11.17 - abrant
	1. Добавлен тип входа "Датчик Холла", он требует питания, которое подается вместо второго канала.
	2. Реализовано переключение типов входа.

31 - 2023.10.13 - abrant
	1. После смены типа входа теперь не нужна перезагрузка attiny.

30 - 2023.08.01 - abrant
	1. Исправлено хранилище. 

29 - 2023.04.15 - neitri, dontsovcmc
	1. Задержка отключения ESP после команды перехода в сон

28 - 2023.04.19 - dontsovcmc
	1. Настройка типа входов

27 - 2023.03.31 - abrant
	1. Исправлен подсчет контрольной суммы. 
	2. Переписано хранилище, теперь хранятся все последние показания и при старте читается блок с верной контрольной суммой и максимальными показаниями.
	3. Добавлено хранение в EEPROM настроек и их получение по I2C

26 - 2023.03.25 - abrant
	1. Исправление потерь импульсов во время связи

25 - 2023.02.02 - abrant
	1. поддержка электронных импульсов

24 - 2022.02.22 - neitri, dontsovcmc
	1. Передача флага о том, что пробуждение по кнопке
	2. Передача количества включений режима настройки
	3. Убрано измерение напряжение, пусть его считает ESP

23 - ветка "8times" - 8 раз в секунду проверка входов

22 - 2021.07.13 - dontsovcmc
	1. переписана работа с watchdog: чип перезагрузится в случае сбоя

21 - 2021.07.01 - dontsovcmc
	1. переписана работа с watchdog
	2. поле voltage стало uint16 (2 байта от uint32 пустые для совместимости с 0.10.3)
	3. период пробуждения 15 мин, от ESP получит 1440 или другой.

20 - 2021.05.31 - dontsovcmc
	1. atmelavr@3.3.0
	2. конфигурация для attiny45

19 - 2021.04.03 - dontsovcmc
	1. WDTCR = _BV( WDCE ); в resetWatchdog

18 - 2021.04.02 - dontsovcmc
	1. WDTCR |= _BV( WDIE ); в прерывании

17 - 2021.04.01 - dontsovcmc
	1. Рефакторинг getWakeUpPeriod

16 - 2021.03.29 - dontsovcmc
	1. Отключение подтягивающих резисторов в I2C (ошибка в tinycore)
	2. Отключение ESP с задержкой 100мс после получения команды на сон (потребление ESP ниже на 7мкА).

15 - 2021.02.07 - kick2nick
	Время пробуждения ESP изменено с 1 суток (1440 мин.) на настриваемое значение
	1. Добавил период пробуждения esp.
	2. Добавил команду приема периода пробуждения по I2C.

14 - 2020.11.09 - dontsovcmc
	1. поддержка attiny84 в отдельной ветке

13 - 2020.06.17 - dontsovcmc
	1. изменил формулу crc
	2. поддержка версии на 4 счетчика (attiny84)
	   -D BUILD_WATERIUS_4C2W

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

static CounterB counter0(4, 2, 3); 	// Вход 1, Blynk: V0, горячая вода PB4 ADC2
static CounterB counter1(3, 3); 	// Вход 2, Blynk: V1, холодная вода (или лог) PB3 ADC3

static ButtonB button(2);  // PB2 кнопка (на линии SCL)
						   // Долгое нажатие: ESP включает точку доступа с веб сервером для настройки
						   // Короткое: ESP передает показания
static ESPPowerPin esp(1); // Питание на ESP

// Данные
struct Header info = 
{
	FIRMWARE_VER, // version
	WATERIUS_MODEL, // model
	0, // service (boot)
	0, // voltage 
	{  // Config
		0, // setup_started_counter
		0, // resets
		{ counter0.type, counter1.type }, 
		DEFAULT_AVR_VCC_REFERENCE_MV // v_reference
	}, 
	{0, 0}, // Data
	{0, 0}, // ADCLevel
	0, // crc
	0  // reserved
};

uint32_t wakeup_period;   // период пробуждения ESP, мин

//Кольцевой буфер для хранения показаний на случай замены питания или перезагрузки
//Кольцовой нужен для того, чтобы превысить лимит записи памяти в 100 000 раз
//Записывается каждый импульс, поэтому для 10л/импульс срок службы памяти 10 000м3
// 100к * 20 = 2 млн * 10 л / 2 счетчика = 10 000 000 л или 10 000 м3
static EEPROMStorage<Data> storage(20); // 8 byte * 20 + crc * 20
static EEPROMStorage<Config> config(2, storage.size()); // 5 byte * 2 + crc * 2

SlaveI2C slaveI2C;

volatile uint32_t 		wdt_count;
volatile CounterEvent 	event;
volatile uint8_t		storage_write_limit = 0; 

/* Вектор прерываний сторожевого таймера watchdog */
ISR(WDT_vect)
{
	++wdt_count;
	event = CounterEvent::TIME;
	if (storage_write_limit > 0)
	{
		storage_write_limit--;
	}
}

/* Вектор прерываний Pin Change */
ISR(PCINT0_vect)
{
	event = CounterEvent::FRONT;
}

// Проверяем входы на замыкание.
// Замыкание засчитывается только при повторной проверке.
inline void counting(CounterEvent ev)
{
	if (counter0.is_impuls(ev))
	{
		info.data.value0++; 				//нужен т.к. при пробуждении запрашиваем данные
		info.adc.adc0 = counter0.adc;
#ifdef LOG_ON
		LOG(F("Input0:"));
		LOG(info.data.value0);
		LOG(F("ADC0:"));
		LOG(info.adc.adc0);
#endif
		if (storage_write_limit == 0)
		{
			storage.add(info.data);
			storage_write_limit = 60*4;		// пишем в память не чаще раза в минуту
		}
	}
#ifndef LOG_ON
	if (counter1.is_impuls(ev))
	{
		info.data.value1++;
		info.adc.adc1 = counter1.adc;
		if (storage_write_limit == 0)
		{
			storage.add(info.data);
			storage_write_limit = 60*4;		// пишем в память не чаще раза в минуту
		}
	}
#endif

#ifdef COUNTER_DEBUG
	noInterrupts();
	if (counter0.on_time || counter1.on_time)
    	PORTB |= _BV(1);
	else
        PORTB &= ~_BV(1);
	interrupts();
#endif

	adc_disable();
	power_adc_disable();
}

void saveConfig()
{
	// записываем 2 раза чтобы полностью переписать хранилище
	config.add(info.config);
	config.add(info.config);	
}


void extendWakeUpPeriod()
{
	esp.extend_wake_up();
}

// Запрос на измерение напряжения или на калибровку (если передано текущее напряжение)
void measureVoltage(uint16_t vcc_real_mv)
{
	info.voltage = readVcc(info.config.v_reference);
	if (vcc_real_mv > 0 && info.voltage > 0) 
	{	
		// чтобы усреднить помехи
		info.voltage = (info.voltage + readVcc(info.config.v_reference)) / 2;
		info.voltage = (info.voltage + readVcc(info.config.v_reference)) / 2;
		// калибровка
    	info.config.v_reference = (uint32_t)info.config.v_reference * vcc_real_mv / info.voltage;
		saveConfig();
	}
}


// Запрос периода при инициализции. Также период может изменится после настройки.
// Настройка. Вызывается однократно при запуске.
void setup()
{
	noInterrupts();
	info.service = MCUSR; // причина перезагрузки
	MCUSR = 0;			  // без этого не работает после перезагрузки по watchdog
	wdt_disable();
	wdt_enable(WDTO_250MS);
	interrupts();

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	if (config.init())
	{
		// Конфигурация найдена
		config.get(info.config);
		info.config.resets++;
		counter0.set_type((CounterType)info.config.types.type0);
		counter1.set_type((CounterType)info.config.types.type1);
	}
	else
	{
		// Конфигурации нет или повреждена
		info.config.resets = 0;
		info.config.setup_started_counter = 0;
		info.config.types.type0 = counter0.type;
		info.config.types.type1 = counter1.type;
		info.config.v_reference = DEFAULT_AVR_VCC_REFERENCE_MV;
	}
	saveConfig();

	if (storage.init())
	{
		storage.get(info.data);
	}

	wakeup_period = WAKEUP_PERIOD_DEFAULT;
	LOG_BEGIN(9600);
	LOG(F("==== START ===="));
	LOG(F("MCUSR"));
	LOG(info.service);
	LOG(F("RESET"));
	LOG(info.config.resets);
	LOG(F("EEPROM used:"));
	LOG(storage.size() + config.size());
	LOG(F("Data:"));
	LOG(info.data.value0);
	LOG(info.data.value1);
}

void counting_1ms(uint8_t &delay_loop_count)
{
	wdt_reset();
	if (delay_loop_count < 250)
	{
		delay_loop_count++;
	}
	else
	{
		// Получаем период опроса входов 250мс, как и от ватчдога
		delay_loop_count = 0;
		event = CounterEvent::TIME;
	}
	if (event != CounterEvent::NONE)
	{
		// Если получили фронт изменения сигнала или набежало время - проверяем входы
		noInterrupts();
		CounterEvent ev = event;
		event = CounterEvent::NONE;
		interrupts();
		counting(ev);
	}
	delayMicroseconds(1000);
}

// Главный цикл, повторящийся раз в сутки или при настройке вотериуса
void loop()
{
	power_all_disable(); 		// Отключаем все лишнее: ADC, Timer 0 and 1, serial interface

	GIMSK = _BV(PCIE);			// Включаем прерывания по фронту счетчиков и кнопки
	PCMSK = _BV(PCINT2);

	counter0.set_type((CounterType)info.config.types.type0);
	counter1.set_type((CounterType)info.config.types.type1);

	// Нажатие кнопки очищаем
	button.init();

	wdt_count = 0;
	while ((wdt_count < wakeup_period) && !button.pressed(event))
	{
		noInterrupts();
		CounterEvent ev = event;
		event = CounterEvent::NONE;
		interrupts();

		counting(ev);

		if (event == CounterEvent::NONE)
		{
			WDTCR |= _BV(WDIE);
			sleep_mode();
		}
		LOG_BEGIN(9600);
	}

	storage.add(info.data);
	power_all_enable();

	LOG_BEGIN(9600);
	LOG(F("Data:"));
	LOG(info.data.value0);
	LOG(info.data.value1);

	// Если пользователь нажал кнопку SETUP, ждем когда отпустит
	// иначе ESP запустится в режиме программирования (кнопка на i2c и 2 пине ESP)
	// Если кнопка не нажата или нажата коротко - передаем показания

	unsigned long wake_up_limit;   // период бодрствования ESP, мсек
	if (button.press == ButtonPressType::LONG)
	{ 
		LOG(F("SETUP pressed"));
		slaveI2C.begin(SETUP_MODE);
		wake_up_limit = SETUP_TIME_MSEC; // 10 мин при настройке

		info.config.setup_started_counter++;
		saveConfig();
	}
	else
	{
		if (button.press == ButtonPressType::SHORT)
		{
			LOG(F("Manual transmit wake up"));
			slaveI2C.begin(MANUAL_TRANSMIT_MODE);
		}
		else
		{
			LOG(F("wake up for transmitting"));
			slaveI2C.begin(TRANSMIT_MODE);
		}
		wake_up_limit = WAIT_ESP_MSEC;
	}

	esp.power(true);

	info.voltage = readVcc(info.config.v_reference); // Пока ESP загружается прочитаем Vcc
	LOG(F("ESP turn on"));

	uint8_t delay_loop_count = 0;		
	while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(wake_up_limit))
	{
		counting_1ms(delay_loop_count);
	}

	slaveI2C.end(); // выключаем i2c slave.

	uint16_t sleep_delay_ms = 20;
	while (sleep_delay_ms--) {
		counting_1ms(delay_loop_count);
	}

	if (!slaveI2C.masterGoingToSleep())
	{
		LOG(F("ESP wake up fail"));
	}
	else
	{
		LOG(F("Sleep received"));
	}

	esp.power(false);

#if WATERIUS_MODEL == MODEL_MINI 
	button.pull_down();
 
	sleep_delay_ms = 300;  // ждём разряда конденсатора 100мкф через кнопку и резистор 3.3кОм + 30 ом (пин-gnd)
	while (sleep_delay_ms--) {
		counting_1ms(delay_loop_count);
	}
#endif

}
