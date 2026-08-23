#ifndef _WATERIUS_CORE_ROUTING_h
#define _WATERIUS_CORE_ROUTING_h

/*
Куда устройство отправляет показания: три независимых транспорта
(waterius.ru, свой HTTP-сервер, MQTT) плюс автодискавери Home Assistant
поверх MQTT.

Каждый транспорт включается своим флажком, но одного флажка мало: без
адреса (и ключа для waterius.ru) отправлять некуда. Транспорты
независимы — выключенный waterius.ru не должен выключать MQTT.

Часть чистого ядра src/core: без Arduino.h.

Транспорт можно выкинуть ещё и на этапе компиляции флагами
WATERIUS_RU_DISABLED, HTTPS_DISABLED, MQTT_DISABLED — это решается в
senders/, здесь только настройки пользователя.
*/

#include "types.h"

/* Настроена ли передача на официальный сайт */
bool is_waterius_site(const Settings &sett);

/* Настроена ли передача на свой веб сервер */
bool is_http(const Settings &sett);

/* Настроена ли интеграция с MQTT */
bool is_mqtt(const Settings &sett);

/* Настроена ли интеграция с Home Assistant */
bool is_ha(const Settings &sett);

/* Будет ли использоваться DHCP */
bool is_dhcp(const Settings &sett);

#endif
