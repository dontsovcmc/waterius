#ifndef _SETUP_h
#define _SETUP_h

#include <Arduino.h>
#include <TinyDebugSerial.h>

// Включение логгирования с TinySerial: 3 pin TX -> RX (TTL-USB 3.3 или 5в), 9600 8N1
// При логгировании не работает счетчик на 3-м пине.
#define DEBUG 

#define ESP_RESET_PIN 1			// Номер пина, которым будим ESP8266. Если менять на 3/4, то нужно поменять пины в прерываниях.

const uint8_t DEVICE_ID = 1;                // Модель устройства

const uint8_t GIVEUP_ON_MASTER_AFTER = 4;	// Сколько секунд ждем передачи данных в ESP

const uint16_t WAKE_MASTER_EVERY_MIN = 30;	// Период передачи данных на сервер, мин
const uint16_t MEASUREMENT_EVERY_MIN = 1;	// Период измерений данных. Кратно минутам строго!

#define STORAGE_SIZE 120  //байт. Размер кольцевого буфера для измерений (измерение=4 байта)

enum State { 
	SLEEP, //глубокий сон
	MEASURING, //сохраняем измерение
	MASTER_WAKE, //пробуждаем ESP8266, i2c
	SENDING //ждем от ESP8266 команды, i2c
};

// attiny не поддерживает 4байт, используем 2байт
struct SlaveStats {
	uint16_t bytesReady; 
	uint8_t  deviceID;
	uint16_t masterWakeEvery;
	uint16_t measurementEvery;
	uint16_t vcc;
	uint8_t dummy;
};

#ifdef DEBUG
  extern TinyDebugSerial mySerial;
  #define DEBUG_CONNECT(x)  mySerial.begin(x)
  #define DEBUG_PRINT(x)    mySerial.print(x)
  #define DEBUG_PRINTLN(x)    mySerial.println(x)
  #define DEBUG_FLUSH()     mySerial.flush()
#else
  #define DEBUG_CONNECT(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x) 
  #define DEBUG_FLUSH()
#endif

#endif

