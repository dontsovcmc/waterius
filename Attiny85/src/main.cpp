#include "Setup.h"

#include <avr/pgmspace.h>
#include <Wire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "counter.h"
#include "button.h"
#include "alarm.h"
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>

// Для логирования раскомментируйте LOG_ON в Setup.h
#if defined(LOG_ON)
TinyDebugSerial mySerial;
#endif


/*
Версии прошивок

41 - 2026.08.26 - dontsov
	1. Детекция аварий по импульсам счётчика. Issue #202.
	   Большой расход: интервал между импульсами короче порога.
	   Протечка: расход не прекращается дольше заданного времени - считается
	   по ритму импульсов, а не по объёму за окно, иначе капающий кран (один
	   импульс в 10 минут) не детектировался бы вовсе.
	   Тревога прерывает сон и поднимает ЕСП вне расписания (ALARM_MODE).
	2. Новые типы входа: LEAKAGE (5) - проводной датчик протечки, замыкание -
	   тревога, импульсы не считаются; LEAKAGE_NC (6) - то же для нормально
	   замкнутого датчика, где тревога размыкание. У второго обрывом провода
	   становится авария, а не тишина.
	3. Команда 'A' - пороги тревог от ЕСП, живут в ОЗУ как период пробуждения.
	   Команда 'K' - ЕСП подтверждает, что доложила о тревоге получателю. Без
	   подтверждения будим её снова, до ALARM_MAX_TRIES раз.
	4. Внеплановых сеансов не больше ALARM_MAX_SESSIONS на период пробуждения.
	   Пауза задаёт темп, бюджет - потолок: дребезжащий датчик иначе выдавал бы
	   новости бесконечно, а счёт попыток начинался бы заново на каждой.
	5. Тревоги уложены в свободные биты Header.flags: размер Header прежний.

40 - 2026.08.25 - dontsov
	1. В Header появился байт флагов (бывший reserved2, у старых прошивок
	   всегда 0). Бит 0 - питание ЕСП подано дольше 5 секунд. Issue #354.
	   ЕСП грузится за ~300мс, поэтому "давно" на первом же её запросе
	   означает, что она перезагрузилась при живом питании, и портал может
	   сказать об этом пользователю.
	   Отсчёт идёт от отдельной метки времени: wake_up_timestamp сдвигает
	   команда 'E' на каждое действие пользователя в портале.

39 - 2026.08.23 - dontsov
	1. Электронный вход считает фронт, а не уровень. Issue #379.
	   Уровень снимается в ISR(PCINT0_vect) и защёлкивается: импульс газового
	   счётчика Гранд длится 0,7-1,5мс и до пробуждения главного цикла не
	   доживал. Мёртвое время после импульса - один тик опроса (250мс).
	2. Электронный вход больше не меряет АЦП: у цифрового входа уровень
	   замыкания ничего не значит, а измерение стоит ~100мкс и тока.
	3. Новый тип входа ELECTRONIC_HIGH (4): импульс - подъём линии, подтяжка
	   выключена. Вход становится высокоомным - требование Гранда (>=1 МОм)
	   выполняется без переделки платы.
	4. Смена типа входа снимает PCMSK: раньше после перехода с электронного
	   на дискретный вход продолжал будить attiny каждым фронтом.

38 - 2026.06.02 - dontsov
	1. 250мс замыкание + 50мс подтверждение = импульс. Issue #371.
	   Было: 2 замыкания подряд, т.е. геркон должен быть замкнут ~500мс - на счетчиках
	   Ителма импульсы пропускались. Теперь при первом замыкании ждем 50мс и перечитываем
	   вход: замкнут - импульс, разомкнулся - помеха, игнорируем.
	   Период опроса входов прежний, 250мс. Импульс засчитывается сразу, а не в конце.

33 - 2025.09.29 - dontsov
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

static ESPPowerPin esp(1); // Питание на ESP

// Детекция аварий (issue #202). Счётчик минут, пауза и доклад - общие на оба
// канала: тревоги уезжают одним сеансом, а ОЗУ у attiny 512 байт
static AlarmDetector alarm0;
static AlarmDetector alarm1;
static AlarmReport alarm_report;
static uint8_t alarm_minute_ticks = 0;
static uint8_t alarm_hold_min = 0;

#if WATERIUS_MODEL == WATERIUS_MODEL_1
static ButtonB button(2);  // PB2 кнопка (на линии SCL)
                           // Долгое нажатие: ESP включает точку доступа с веб сервером для настройки
                           // Короткое: ESP передает показания
#endif
#if WATERIUS_MODEL == WATERIUS_MODEL_2
static ButtonB2 button(1);  // PB1 кнопка (на линии wakeup)
                           // Долгое нажатие: ESP включает точку доступа с веб сервером для настройки
                           // Короткое: ESP передает показания
#endif

// Данные - designated initializers для битовых полей
struct Header info = {
    .version = FIRMWARE_VER,
    .service = 0,
    .on_pulse0 = 0,
    .on_pulse1 = 0,
    .voltage = 0,
    .flags = 0,
    .config = {0, 0, WATERIUS_MODEL, {counter0.type, counter1.type}},
    .data = {0, 0},
    .adc = {0, 0},
    .crc = 0,
    .reserved3 = 0
};

uint32_t wakeup_period;

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
	// Уровень входов снимаем прямо здесь: импульс электронного счётчика
	// бывает короче миллисекунды (у газового Гранда 0,7мс, issue #379) и до
	// пробуждения главного цикла не доживает. Снимок один на оба счётчика.
	const uint8_t pins = PINB;
	counter0.on_front(pins);
	counter1.on_front(pins);

	event = CounterEvent::FRONT;
}

/*
Ход времени для детекции аварий (issue #202).

Тик опроса входов - 250мс, он же единственная мера времени у attiny: и во сне,
и во время сеанса с ЕСП. Минута считается одним счётчиком на оба канала.
*/
inline void alarm_tick(CounterEvent ev)
{
	/*
	Только по тику опроса. По фронту CounterB::discrete выходит, не читая вход,
	поэтому on_pulse там ещё прошлый - а у нормально-замкнутого датчика прошлое
	значение на старте это ложная тревога: до первого опроса on_pulse нулевой,
	то есть "разомкнут".
	*/
	if (ev != CounterEvent::TIME)
		return;

	/*
	Нормально-замкнутый датчик (LEAKAGE_NC) - то же состояние наоборот: тревога
	не замыкание, а размыкание. Заодно тревогой становится обрыв провода,
	который у нормально-разомкнутого неотличим от тишины.
	*/
	if (counter0.type == CounterType::LEAKAGE)
		alarm0.set_wet(counter0.on_pulse);
	else if (counter0.type == CounterType::LEAKAGE_NC)
		alarm0.set_wet(!counter0.on_pulse);

	if (counter1.type == CounterType::LEAKAGE)
		alarm1.set_wet(counter1.on_pulse);
	else if (counter1.type == CounterType::LEAKAGE_NC)
		alarm1.set_wet(!counter1.on_pulse);

	alarm0.on_tick();
	alarm1.on_tick();

	if (++alarm_minute_ticks < ALARM_TICKS_PER_MINUTE)
		return;

	alarm_minute_ticks = 0;
	alarm0.on_minute();
	alarm1.on_minute();

	if (alarm_hold_min)
		alarm_hold_min--;
}

// Проверяем входы на замыкание.
// Замыкание засчитывается только при повторной проверке.
inline void counting(CounterEvent ev)
{
	if (counter0.is_impuls(ev))
	{
		info.data.value0++; 				//нужен т.к. при пробуждении запрашиваем данные
		info.adc.adc0 = counter0.adc;
		alarm0.on_pulse();
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
	info.on_pulse0 = counter0.on_time > 0;
#ifndef LOG_ON
	if (counter1.is_impuls(ev))
	{
		info.data.value1++;
		info.adc.adc1 = counter1.adc;
		alarm1.on_pulse();
		if (storage_write_limit == 0)
		{
			storage.add(info.data);
			storage_write_limit = 60*4;		// пишем в память не чаще раза в минуту
		}
	}
	info.on_pulse1 = counter1.on_time > 0;
#endif

	alarm_tick(ev);

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

/*
Питание ЕСП подано дольше ESP_POWERED_LONG_MSEC.

Зовётся из SlaveI2C при сборке Header. Если ЕСП спрашивает данные первый раз
после своей загрузки, а мы отвечаем "давно", значит она перезагрузилась при
живом питании: вывод делает сама ЕСП, attiny только измеряет время (#354).
*/
bool is_esp_powered_long()
{
	return esp.powered_longer_than(ESP_POWERED_LONG_MSEC);
}

/*
Состояние тревог обоих каналов одним байтом (issue #202).

Зовётся из SlaveI2C при сборке Header: главному циклу вести этот байт незачем.
Раскладка - HEADER_ALARM_* в Setup.h.
*/
uint8_t alarm_bits()
{
	return (uint8_t)(alarm0.state << HEADER_ALARM_SHIFT0) |
		   (uint8_t)(alarm1.state << HEADER_ALARM_SHIFT1);
}

/*
Пороги тревог от ЕСП: по два uint16 на канал, старшим байтом вперёд.
*/
void set_alarm_config(const uint8_t *data)
{
	alarm0.configure((uint16_t)(data[0] << 8) | data[1],
					 (uint16_t)(data[2] << 8) | data[3]);
	alarm1.configure((uint16_t)(data[4] << 8) | data[5],
					 (uint16_t)(data[6] << 8) | data[7]);
}

/*
ЕСП доложила о тревоге получателю: повторять сеанс не нужно.
*/
void confirm_alarm()
{
	alarm_report.confirm();
}

/*
Есть ли повод разбудить ЕСП вне расписания.

Три условия. Пауза после сеанса задаёт темп: для подтверждённого доклада это
защита от дребезга датчика протечки, для неподтверждённого - интервал до
следующей попытки. Бюджет задаёт потолок: пауза одна лишь ограничивает частоту,
но не количество, а дребезжащий датчик способен выдавать новости бесконечно.
И наконец, собственно новость или неподтверждённый доклад.
*/
inline bool alarm_pending()
{
	return alarm_hold_min == 0 && alarm_report.budget_left() &&
		   (alarm_report.pending || alarm0.changed || alarm1.changed);
}

//Запрос периода при инициализции. Также период может изменится после настройки.
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

	wdt_count = 0;
	while ((wdt_count < wakeup_period) && !button.pressed(event) && !alarm_pending())
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

	unsigned long wake_up_limit = SETUP_TIME_MSEC;  // 10 мин при настройке

	// Разбудила тревога, а не расписание и не кнопка
	const bool alarm_wake = alarm_pending();

	if (button.press == ButtonPressType::LONG)
	{ 
		LOG(F("SETUP pressed"));
		slaveI2C.begin(SETUP_MODE);
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
		else if (alarm_wake)
		{
			wake_up_limit = WAIT_ESP_MSEC;
			LOG(F("Alarm wake up"));
			slaveI2C.begin(ALARM_MODE);
		}
		else
		{
			wake_up_limit = WAIT_ESP_MSEC; // 2 мин при передаче данных
			LOG(F("wake up for transmitting"));
			slaveI2C.begin(TRANSMIT_MODE);
		}
	}

	// Нажатие кнопки обработали и удаляем
	button.reset();
	info.voltage = readVcc(); // Прочитаем Vcc после включения ESP

	/*
	Любой сеанс показывает серверу текущее состояние тревог: Header ЕСП читает
	всегда. Поэтому новость считается отданной независимо от режима, а ждём мы
	не сеанса, а подтверждения доставки.

	Иначе после планового сеанса, увёзшего свежую тревогу, флаг "есть новость"
	остался бы висеть, и устройство проснулось бы ещё раз - доложить уже
	доложенное.
	*/
	if (alarm0.changed || alarm1.changed)
	{
		alarm_report.start();
		alarm0.changed = 0;
		alarm1.changed = 0;
	}
	// Пока ЕСП не подтвердит доставку, состояние не снимаем: иначе тревога
	// успела бы погаснуть до чтения байта флагов
	alarm0.hold(alarm_report.pending);
	alarm1.hold(alarm_report.pending);

	esp.power(true);

	LOG(F("ESP turn on"));

	uint8_t delay_loop_count = 0;		
	while (!slaveI2C.masterGoingToSleep() && !esp.elapsed(wake_up_limit))
	{
		counting_1ms(delay_loop_count);
	}
	uint8_t sleep_delay_ms = DELAY_SENT_SLEEP;
	while (sleep_delay_ms--) {
		counting_1ms(delay_loop_count);
	}

	slaveI2C.end(); // выключаем i2c slave.

	/*
	Итог доклада о тревоге. Квитанцию присылает ЕСП командой 'K', когда данные
	приняты получателем. Не пришла - пробуем ещё, но не больше ALARM_MAX_TRIES
	раз: без сети сеансы только жгут батарею, а состояние всё равно уедет со
	следующим плановым выходом на связь.
	*/
	if (alarm_report.pending)
	{
		alarm_report.failed();
	}

	/*
	Бюджет внеплановых сеансов. Плановый сеанс увёз текущее состояние, значит
	счёт начинается заново; внеплановый - потрачен.
	*/
	if (alarm_wake)
	{
		alarm_report.spend();
	}
	else
	{
		alarm_report.new_period();
	}

	/*
	Состояние держим, пока доклад не подтверждён: между попытками тревога иначе
	успела бы погаснуть, и на сервер уехало бы "всё хорошо".

	Но только пока мы действительно собираемся доложить. Исчерпали бюджет -
	ближайший сеанс будет плановым, и держать до него нечего: за это время
	датчик успеет высохнуть, и увезти надо правду, а не застывшую тревогу.
	*/
	const bool waiting = alarm_report.pending && alarm_report.budget_left();
	alarm0.hold(waiting);
	alarm1.hold(waiting);

	/*
	Пауза после внепланового сеанса - и до следующей попытки, и просто чтобы
	дребезжащий датчик протечки не поднимал ЕСП по кругу. Без неё связка
	"намок - высох - намок" будила бы устройство на каждом переходе.
	*/
	if (alarm_report.pending || alarm_wake)
	{
		alarm_hold_min = ALARM_HOLD_MIN;
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
}
