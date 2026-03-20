# Архитектурный анализ: MQTT QoS 2 с AsyncMqttClient для Waterius

## Контекст

PubSubClient v2.8.0 не поддерживает QoS для `publish()`/`beginPublish()`. Для реализации гарантированной доставки нужна библиотека **AsyncMqttClient** (marvinroger/async-mqtt-client), которая поддерживает QoS 0/1/2 для публикации.

**Scope**: QoS 1/2 только для данных счётчиков (`publish_data`). Discovery и сервисные сообщения остаются QoS 0.

---

## 1. QoS 2 vs QoS 1 — нужен ли QoS 2 вообще?

| | QoS 1 (at least once) | QoS 2 (exactly once) |
|---|---|---|
| Handshake | 2 пакета: PUBLISH → PUBACK | 4 пакета: PUBLISH → PUBREC → PUBREL → PUBCOMP |
| Латентность | 1 round-trip | 2 round-trips |
| Дубликаты | Возможны | Исключены |
| Для Waterius | Безвредны — показания абсолютные (м³), дубликат просто перезапишется | Не нужно — данные идемпотентны |

**Важный нюанс с `cleanSession=true`**: Если ESP уснёт между PUBREC и PUBCOMP, брокер (Mosquitto) всё равно доставит сообщение подписчикам после PUBREC. Фактически это поведение QoS 1. Это нивелирует саму идею "ровно однажды" для wake-sleep устройства.

**Рекомендация**: QoS 1 более чем достаточен для Waterius. QoS 2 оправдан только если downstream обрабатывает дельты или считает каждое сообщение как событие. Архитектура ниже покрывает оба варианта.

---

## 2. Главная проблема: async API vs синхронный wake-send-sleep

Текущий flow в `main.cpp` — линейный:
```
connect_and_subscribe_mqtt()  → sync, блокирующий
send_mqtt()                   → sync, блокирующий
wifi_shutdown()
deepSleep()
```

AsyncMqttClient — полностью неблокирующий: `connect()`, `publish()`, `subscribe()` возвращаются мгновенно. Результат приходит через callback.

### Решение: синхронные обёртки с yield()

На ESP8266 WiFi стек (lwIP) и ESPAsyncTCP обрабатывают события при вызове `yield()` / `delay(0)`. Паттерн:

```cpp
struct MqttState {
    volatile bool connected = false;
    volatile bool disconnected = false;
    volatile uint8_t pending_qos_count = 0;  // неподтверждённые QoS-пакеты
    volatile bool subscribe_done = false;
    // Буфер для incoming retained-сообщений
    char incoming_topic[128];
    char incoming_payload[256];
    volatile bool has_incoming = false;
};

static MqttState mqtt_state;

// Блокирующее ожидание с таймаутом
bool wait_for(volatile bool &flag, unsigned long timeout_ms) {
    unsigned long start = millis();
    while (!flag) {
        yield();  // отдаём управление TCP стеку
        if (millis() - start > timeout_ms) return false;
    }
    return true;
}

// Ожидание всех PUBACK/PUBCOMP
bool wait_all_published(unsigned long timeout_ms) {
    unsigned long start = millis();
    while (mqtt_state.pending_qos_count > 0) {
        yield();
        if (millis() - start > timeout_ms) return false;
    }
    return true;
}
```

**Ограничение**: callback-и AsyncMqttClient вызываются из контекста TCP стека. Они должны быть максимально легковесными — только флаги и копирование данных. Нельзя вызывать `yield()` из callback-ов.

---

## 3. Трекинг пакетов

Для данных достаточно атомарного счётчика (не нужен полноценный трекинг по packetId):

```cpp
// При публикации
uint16_t id = mqttClient.publish(topic, qos, retain, payload, len);
if (qos > 0 && id != 0) {
    mqtt_state.pending_qos_count++;
}

// В callback onPublish (PUBACK для QoS 1, PUBCOMP для QoS 2)
void onMqttPublish(uint16_t packetId) {
    if (mqtt_state.pending_qos_count > 0)
        mqtt_state.pending_qos_count--;
}
```

Discovery публикуется с QoS 0 — трекинг не нужен.

---

## 4. Приём retained-команд от HA

Текущий `mqtt_callback` синхронно вызывает `publish()` внутри себя. В async-модели callback должен только буферизовать:

```cpp
// callback — только копируем данные
void onMqttMessage(char* topic, char* payload, ..., size_t len, ...) {
    strncpy(mqtt_state.incoming_topic, topic, sizeof(mqtt_state.incoming_topic));
    memcpy(mqtt_state.incoming_payload, payload, len);
    mqtt_state.incoming_payload[len] = 0;
    mqtt_state.has_incoming = true;
}

// Основной поток — обработка
void process_retained_messages(AsyncMqttClient &client, Settings &sett, ...) {
    unsigned long start = millis();
    while (millis() - start < MQTT_RETAINED_WAIT) {
        yield();
        if (mqtt_state.has_incoming) {
            mqtt_state.has_incoming = false;
            String topic(mqtt_state.incoming_topic);
            String payload(mqtt_state.incoming_payload);
            update_settings(topic, payload, sett, data, json_data);
            // Стереть retained
            client.publish(mqtt_state.incoming_topic, 0, true, "", 0);
            start = millis();  // сбрасываем — может прийти ещё
        }
    }
}
```

---

## 5. Новый flow sender_mqtt.h

```
connect_and_subscribe_mqtt():
  1. mqttClient.connect()              // non-blocking
  2. wait_for(connected, 5000ms)       // yield-loop
  3. if auto_discovery:
       mqttClient.subscribe(topic/#, 1)
       wait_for(subscribe_done, 3000ms)
       process_retained_messages()     // yield-loop 500ms

send_mqtt():
  4. if auto_discovery && (SETUP || MANUAL):
       publish_discovery(...)          // всё QoS 0, fire-and-forget
  5. publish_data(...)                 // QoS 1 или 2
  6. wait_all_published(5000ms)        // ← КЛЮЧЕВОЕ: ждём все ACK/PUBCOMP
  7. mqttClient.disconnect()
  8. wait_for(disconnected, 1000ms)
```

Максимальное время MQTT-фазы: ~15 секунд. Сопоставимо с текущим PubSubClient.

---

## 6. Подводные камни

### Flash/RAM
- AsyncMqttClient: ~25-40 KB flash (PubSubClient: ~12 KB). Прирост ~15-28 KB. При 82% (~819 KB) — хватит.
- RAM: ESPAsyncTCP уже в проекте (для WebServer в SETUP_MODE). MQTT работает в TRANSMIT_MODE — не пересекаются.

### ESPAsyncTCP fork
- Проект использует `ESP32Async/ESPAsyncTCP@2.0.0`. AsyncMqttClient (marvinroger) ожидает оригинальный `ESPAsyncTCP`. Нужна проверка совместимости заголовков.

### Streaming API нет
- AsyncMqttClient не имеет `beginPublish/write/endPublish`. Только `publish(topic, qos, retain, payload, len)`. Текущие `publish_big` / `publish_chunked` не нужны — AsyncMqttClient сам буферизует.

### cleanSession и QoS 2
- С `cleanSession=true` (обязательно для wake-sleep) брокер не хранит состояние между соединениями. Если ESP уснёт до PUBCOMP — транзакция не завершится. Mosquitto доставит сообщение уже после PUBREC, что фактически = QoS 1.

### Баг в текущем коде
- `subscribe.cpp:289` — `mqtt_connect()` возвращает `true` после всех неудачных попыток. При миграции баг устраняется автоматически.

---

## 7. Затрагиваемые файлы

| Файл | Изменения |
|---|---|
| `platformio.ini` | +async-mqtt-client, -PubSubClient |
| `senders/sender_mqtt.h` | **Полная перепись**: MqttState, sync-обёртки, callback-и |
| `ha/publish.h` + `.cpp` | **Перепись**: три режима → один `publish()` с QoS |
| `ha/subscribe.h` + `.cpp` | **Существенная перепись**: mqtt_connect/subscribe удаляются, callback буферизуется |
| `ha/publish_data.h` + `.cpp` | Смена сигнатур: `PubSubClient&` → `AsyncMqttClient&`, добавление QoS |
| `ha/publish_discovery.h` + `.cpp` | Смена сигнатур (механическая замена) |
| `ha/discovery_entity.h` | Смена include |
| `main.cpp` | Без изменений — API `connect_and_subscribe_mqtt()` / `send_mqtt()` сохраняется |
| `setup.h` | Без изменений (QoS задаётся define-ом или из reserved-байта) |

---

## 8. Резюме

- **Для Waterius QoS 1 достаточен** — данные идемпотентны, `cleanSession=true` нивелирует преимущества QoS 2
- Миграция на AsyncMqttClient — **средний объём работ** (~6-7 файлов), но архитектурно чистая
- Ключевой паттерн: **sync-обёртки с yield()** + **счётчик pending QoS** + **wait перед disconnect**
- ESPAsyncTCP уже в проекте — дополнительных зависимостей (кроме самого async-mqtt-client) нет
