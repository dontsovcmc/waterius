#include "sync_time.h"
#include "Logging.h"
#include <IPAddress.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include "core/connect_log.h"

// Модуль основан на следующих модулях
// https://github.com/arduino-libraries/NTPClient/blob/master/NTPClient.cpp
// https://github.com/letscontrolit/ESPEasy/blob/mega/src/src/Helpers/ESPEasy_time.cpp
// https://github.com/arendst/Tasmota/blob/development/tasmota/tasmota_support/support_wifi.ino

#define NTP_TIMEOUT 1000              // в первой попытке, дальше растёт
#define NTP_PORT 123
#define USEC 1000000UL
#define MSEC 1000UL
#define TIME_FORMAT "%FT%T%z"
#define UDP_PORT_ATTEMPTS 3
#define NTP_ATTEMPTS 3

// Причина последней неудачи: одна запись на сбой, а попыток до четырёх
#define NTP_ERR_NO_PORT -1
#define NTP_ERR_NOT_SENT -2
#define NTP_ERR_NO_REPLY -3
#define NTP_ERR_BAD_REPLY -4
static int16_t ntp_last_error = 0;

// NTP_PACKET_SIZE — в core/timekeeping.h
uint8_t packet_buffer[NTP_PACKET_SIZE]; // Buffer to hold incoming & outgoing packets

/**
 * @brief Получает следующий ижентификатор в пуле серверов
 *
 * @return int
 */
int get_next_ntp_server_id()
{
    static int ntp_server_id = -1;
    // первый раз сервер выбирается случайным образом из пула,
    // каждый следующий раз будет выбираться следующий в пуле.
    // Верхняя граница random исключается, поэтому NTP_POOL_SIZE, а не -1:
    // иначе последний сервер пула не выбирался бы никогда.
    if (ntp_server_id < 0)
    {
        ntp_server_id = random(0, NTP_POOL_SIZE);
    }
    else
    {
        ntp_server_id = next_ntp_server_id(ntp_server_id);
    }
    return ntp_server_id;
}

/**
 * @brief Получает имя следующего в пуле сервера
 *
 * @return String имя сервера
 */
String get_next_ntp_server_name()
{
    int ntp_server_id = get_next_ntp_server_id();
    String ntp_server_name = String(ntp_server_id);
    ntp_server_name += F(".ru.pool.ntp.org");
    return ntp_server_name;
}

/**
 * @brief Открывает UDP порт для начала передачи
 *
 * @param udp класс для работы с UDP
 * @return true удалось открыть порт
 * @return false Не удалось открыть порт
 */
bool begin_udp_random_port(WiFiUDP &udp)
{
    unsigned int attempts = UDP_PORT_ATTEMPTS;

    do
    {
        long port = random(1025, 65535);
        if (udp.begin(port) != 0)
        {
            return true;
        }
    } while (attempts--);

    return false;
}

/**
 * @brief Парсинг ответа NTP сервера
 *
 * @param udp
 * @param timeout_ms сколько ждать ответ
 * @return uint64_t
 */
uint64_t get_ntp_response(WiFiUDP &udp, const uint32_t timeout_ms)
{
    uint32_t begin_wait = millis();

    while (millis() - begin_wait < timeout_ms)
    {
        uint32_t size = udp.parsePacket();
        uint32_t remote_port = udp.remotePort();

        if ((size >= NTP_PACKET_SIZE) && (remote_port == NTP_PORT))
        {
            udp.read(packet_buffer, NTP_PACKET_SIZE); // Read packet into the buffer
            udp.stop();

            uint64_t ntp_nanos = parse_ntp_packet(packet_buffer, NTP_PACKET_SIZE);
            if (ntp_nanos == 0)
            {
                LOG_ERROR(F("NTP: bad reply"));
                ntp_last_error = NTP_ERR_BAD_REPLY;
                return 0;
            }

            uint32_t total_delay = millis() - begin_wait;

            // compensate for the delay by adding half the total delay
            uint32_t delay_compensation = total_delay / 2;

            LOG_INFO(F("NTP: NTP replied: delay ") << total_delay << F(" mSec"));

            ntp_nanos += (uint64_t)delay_compensation * (NSEC / MSEC);

            return ntp_nanos;
        }
        delay(10);
    }
    // Timeout.
    LOG_ERROR(F("NTP: No reply from NTP server"));
    ntp_last_error = NTP_ERR_NO_REPLY;
    udp.stop();
    return 0;
}

/**
 * @brief Формирование и отправка запроса на получение времени с сервера
 *
 * @param udp
 * @param ntp_server_ip
 * @return true
 * @return false
 */
bool send_ntp_request(WiFiUDP &udp, IPAddress &ntp_server_ip)
{
    while (udp.parsePacket() > 0) // Discard any previously received packets
    {
        yield();
    }

    memset(packet_buffer, 0, NTP_PACKET_SIZE);
    packet_buffer[0] = 0b11100011; // LI, Version, Mode
    packet_buffer[1] = 0;          // Stratum, or type of clock
    packet_buffer[2] = 6;          // Polling Interval
    packet_buffer[3] = 0xEC;       // Peer Clock Precision
    packet_buffer[12] = 49;
    packet_buffer[13] = 0x4E;
    packet_buffer[14] = 49;
    packet_buffer[15] = 52;

    if (udp.beginPacket(ntp_server_ip, NTP_PORT) == 0) // NTP requests are to port 123
    {
        udp.stop();
        return false;
    }

    yield();

    udp.write(packet_buffer, NTP_PACKET_SIZE);
    udp.endPacket();
    return true;
}

/**
 * @brief Возвращает текущее время с сервера NTP в наносекундах
 *
 * @param ntp_server_name  имя сервера NTP
 * @param attempt номер попытки, с единицы
 * @return uint_64t  время с сервера NTP в наносекундах
 */
uint64_t get_ntp_nanos(const String &ntp_server_name, const int attempt)
{
    IPAddress ntp_server_ip;

    if (WiFi.hostByName(ntp_server_name.c_str(), ntp_server_ip) != 1)
    {
        LOG_ERROR(F("NTP: Unable to resolve ") << ntp_server_name);
        ntp_last_error = CONNECT_CODE_DNS;
        return 0;
    }

    LOG_INFO(F("NTP: NtpServer ") << ntp_server_name << F(" IP ") << ntp_server_ip.toString());

    yield();

    WiFiUDP udp;

    if (!begin_udp_random_port(udp))
    {
        LOG_ERROR(F("NTP: Unable to open udp port "));
        ntp_last_error = NTP_ERR_NO_PORT;
        return 0;
    }

    yield();

    if (!send_ntp_request(udp, ntp_server_ip))
    {
        LOG_ERROR(F("NTP: Unable to send"));
        ntp_last_error = NTP_ERR_NOT_SENT;
        return 0;
    }

    yield();

    uint64_t ntp_nanos = get_ntp_response(udp, NTP_TIMEOUT * attempt);

    yield();

    return ntp_nanos;
}

/**
 * @brief Синхронизирует время c указанным сервером
 *
 * @param ntp_server_name имя сервера
 * @param attempt номер попытки, с единицы
 * @return true время синхронизировано
 * @return false время НЕ синхронизировано
 */
bool sync_ntp_time(const String &ntp_server_name, const time_t known_good, const int attempt)
{
    struct timeval tv;
    // Часы приходится выставить до запроса, и при неудаче устройство останется
    // с этим значением. Ставим последнее достоверно известное время, иначе
    // получится now < last_send — "проснулись раньше, чем засыпали".
    tv.tv_sec = known_good;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    uint32_t start_time = millis();

    uint64_t ntp_nanos = get_ntp_nanos(ntp_server_name, attempt);

    tv.tv_sec = ntp_nanos / NSEC;
    tv.tv_usec = (ntp_nanos % NSEC) / 1000;

    if (is_valid_time(tv.tv_sec))
    {
        settimeofday(&tv, NULL);
        LOG_INFO(F("NTP: Time synced ") << millis() - start_time << F(" msec"));
        LOG_INFO(F("NTP: Current time ") << get_current_time());
        return true;
    }
    else
    {
        LOG_ERROR(F("NTP: Unable to sync time"));
        if (ntp_nanos)
        {
            ntp_last_error = NTP_ERR_BAD_REPLY;
        }
        return false;
    }
}

/**
 * @brief Синхронизация времени по протоколу NTP
 *
 * @return true время синхронизировано
 * @return false время НЕ синхронизировано
 */
bool sync_ntp_time(const time_t known_good)
{
    uint32_t start_time = millis();
    LOG_INFO(F("NTP: Sync time..."));
    String ntp_server_name;

    // Каждая попытка идёт к другому серверу пула:
    // - RFC 4330: нельзя опрашивать сервер чаще раза в 15 с. Повторять запрос тому же
    //   серверу он советует только если запасного нет, а при запасном велит чередовать.
    // - RFC 8633: держать несколько источников, чтобы пережить отказ одного.
    // - Правила pool.ntp.org: не использовать приёмы вроде burst, это только лишняя
    //   нагрузка на серверы волонтёров. Если сервер перестал отвечать, надо заново
    //   запросить DNS.
    for (int attempt = 1; attempt <= NTP_ATTEMPTS; attempt++)
    {
        LOG_INFO(F("NTP: Attempt #") << attempt << F(" from ") << NTP_ATTEMPTS);
        ntp_server_name = get_next_ntp_server_name();
        if (sync_ntp_time(ntp_server_name, known_good, attempt))
        {
            LOG_INFO(F("NTP: Time successfully synced. Total time spent ") << millis() - start_time << F(" msec"));
            return true;
        };
    }

    LOG_ERROR(F("NTP: Time could not synced. Total time spent ") << millis() - start_time << F(" msec"));
    return false;
}

/**
 * @brief Синхронизирует время по настройкам пользователя
 *
 * @param sett настройки устройства
 * @return true время синхронизировано
 * @return false время НЕ синхронизировано
 */
bool sync_ntp_time(const Settings &sett)
{

    String ntp_server = sett.ntp_server;
    const time_t known_good = clock_before_sync(sett.last_send);

    if (sett.ntp_server[0] && !ntp_server.equalsIgnoreCase(String(DEFAULT_NTP_SERVER))) // проверяем что сервер указан и не равняется по умолчанию
    {
        // Пробуем получить время с пользовательского сервера
        if (sync_ntp_time(ntp_server, known_good))
        {
            return true;
        }
    }
    // если не удалось с пользовательского сервера получить время
    // то берем время по серврам из пула
    return sync_ntp_time(known_good);
}

/**
 * @brief Синхронизирует время, если пришёл срок
 *
 * @param sett настройки устройства
 * @return true время получено от NTP-сервера прямо сейчас
 * @return false синхронизация не потребовалась или не удалась
 */
bool maybe_sync_time(Settings &sett)
{
    // Ручное пробуждение кнопкой задаёт base_time заново, то есть определяет
    // расписание на будущее. Строить его по оценке нельзя — ошибка оценки
    // осталась бы в расписании навсегда. Батарею тут беречь не от кого:
    // пользователь стоит рядом и жмёт кнопку.
    const bool forced = sett.mode != TRANSMIT_MODE;

    if (forced || need_ntp_sync(sett.last_time_sync, sett.wakeups_since_sync,
                                sett.ntp_error_counter, sett.wakeup_per_min, sett.ntp_sync_count))
    {
        if (sync_ntp_time(sett))
        {
            sett.ntp_error_counter = 0;
            if (sett.ntp_sync_count < NTP_WARMUP_SYNCS)
            {
                sett.ntp_sync_count++;
            }
            return true;
        }

        // Счётчик неудач подряд задаёт паузу перед следующей попыткой.
        // Насыщаем: переполнение обнулило бы паузу, и устройство с лежащим
        // роутером снова начало бы опрашивать NTP каждое пробуждение.
        if (sett.ntp_error_counter < 0xFF)
        {
            sett.ntp_error_counter++;
        }
        LOG_ERROR(F("NTP: sync failed, failures in row: ") << sett.ntp_error_counter);
        connect_log_push(sett, CONNECT_NTP, ntp_last_error);
        return false;
    }

    // Срок не подошёл. Часы всё равно надо выставить: иначе метки времени
    // в MQTT и в логах будут из 1970 года.
    const time_t estimated = estimate_time(sett.last_time_sync, sett.wakeups_since_sync,
                                           sett.wakeup_per_min);
    if (is_valid_time(estimated))
    {
        struct timeval tv;
        tv.tv_sec = estimated;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        LOG_INFO(F("NTP: sync not due, wakeups since sync: ") << sett.wakeups_since_sync);
        LOG_INFO(F("NTP: estimated time ") << get_current_time());
    }

    return false;
}

/**
 * @brief Получает текущее время
 *
 * @return строка с временем в формате C
 */
String get_current_time()
{
    char buf[100];
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    // ISO8601 date time string format (2019-11-29T23:29:55+0800).
    strftime(buf, sizeof(buf), TIME_FORMAT, &timeinfo);
    return String(buf);
}

/**
 * @brief Проверка валидно ли время,
 * дата должна быть больше чем 1 Января 2020 года
 *
 * @param time
 * @return true время валидно
 * @return false время невалидно
 */
// is_valid_time() — в core/timekeeping.cpp
