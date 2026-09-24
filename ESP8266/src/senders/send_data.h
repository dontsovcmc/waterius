#ifndef SEND_DATA_H_
#define SEND_DATA_H_

#include "setup.h"
#include <ArduinoJson.h>
#include "master_i2c.h"
#include "core/blink.h"

/*
Итог отправки складывается в status, у каждого получателя своё поле:
status.waterius, status.http, status.mqtt. Отсюда светодиод узнаёт, чем
закончился сеанс (облачных двоих сводит core/blink.h:cloud_status), а
core/alarm.h:alarm_delivered - докладывать ли attiny о доставке тревоги.
*/
void send_data(Settings &sett, const AttinyData &data, const CalculatedData &cdata, JsonDocument &json_data, JsonDocument &json_settings, SessionStatus &status);
bool settings_received(const JsonDocument &json_settings_received);

inline bool has_ota(const JsonDocument &json_settings_received)
{
    return json_settings_received.containsKey(F("ota"));
}

/*
Сколько раз за сеанс применяем присланное. Второй круг нужен команде, которая
приехала, пока уходила посылка с новыми значениями, третий - страховка от
третьей подряд. Больше не имеет смысла: сеанс не резиновый.
*/
#define MQTT_APPLY_PASSES 3

// Сколько ждать опоздавшую команду перед закрытием сеанса с брокером
#define MQTT_LATE_WAIT_MS 500

#ifndef MQTT_DISABLED
bool connect_and_subscribe_mqtt(Settings &sett, JsonDocument &json_settings_received);
void disconnect_mqtt(const Settings &sett);

/*
Качает сокет заданное время и говорит, приехала ли команда: применение в
сеансе одно, и пришедшая после него иначе пропала бы.
*/
bool mqtt_late_commands(uint16_t msec);

// Снять у брокера удерживаемые команды, которые уже применены
void forget_applied_commands();

// Снять признак прихода команды: то, что приедет позже, получит свой круг
void note_settings_applied();
#else
inline void note_settings_applied() {}
#endif

#endif
