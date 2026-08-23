#include "timekeeping.h"

bool is_valid_time(const time_t time)
{
    return time > (time_t)START_VALID_TIME;
}

time_t clock_before_sync(const time_t last_send)
{
    return is_valid_time(last_send) ? last_send : (time_t)START_VALID_TIME;
}

uint64_t parse_ntp_packet(const uint8_t *packet, const size_t size)
{
    if (packet == nullptr || size < NTP_PACKET_SIZE)
        return 0;

    if ((packet[0] & 0b11000000) == 0b11000000)
    {
        // Leap-Indicator: unknown (clock unsynchronized)
        // See: https://github.com/letscontrolit/ESPEasy/issues/2886#issuecomment-586656384
        return 0;
    }

    // convert four bytes starting at location 40 to a long integer
    // TX time is used here.
    uint32_t secs_since_1900 = (uint32_t)packet[40] << 24;
    secs_since_1900 |= (uint32_t)packet[41] << 16;
    secs_since_1900 |= (uint32_t)packet[42] << 8;
    secs_since_1900 |= (uint32_t)packet[43];

    // Ноль означает "сервер не проставил время". Любое значение до эпохи unix
    // означает битый пакет или чужой UDP-ответ на наш порт: вычитание 70 лет
    // ушло бы в переполнение uint64, и часы уехали бы в 2484 год. Оттуда
    // значение попадает в last_send в EEPROM, и now < last_send становится
    // постоянным состоянием, а не разовым сбоем.
    if (secs_since_1900 <= SEVENTY_YEARS)
        return 0;

    uint32_t tmp_fraction = (uint32_t)packet[44] << 24;
    tmp_fraction |= (uint32_t)packet[45] << 16;
    tmp_fraction |= (uint32_t)packet[46] << 8;
    tmp_fraction |= (uint32_t)packet[47];
    uint32_t fraction = (((uint64_t)tmp_fraction) * NSEC) >> 32;

    return (((uint64_t)secs_since_1900) - SEVENTY_YEARS) * NSEC + fraction;
}

int next_ntp_server_id(const int current)
{
    if (current < 0)
        return 0;

    return (current + 1) % NTP_POOL_SIZE;
}

uint16_t bump_wakeups(const uint16_t wakeups)
{
    return wakeups == 0xFFFF ? wakeups : (uint16_t)(wakeups + 1);
}

/*
Через сколько пробуждений подходит срок очередной синхронизации.
Округление вверх: лучше синхронизироваться чуть позже срока, чем чуть раньше.
*/
static uint32_t sync_deadline_wakeups(const uint16_t wakeup_per_min)
{
    const uint32_t period_sec = (wakeup_per_min > 0 ? wakeup_per_min : 1) * 60UL;

    return (NTP_SYNC_INTERVAL_SEC + period_sec - 1) / period_sec;
}

/*
Сколько пробуждений пропустить из-за неудачных попыток: 2 после первой,
ещё 4 после второй, 8 после третьей и так далее до потолка.
*/
static uint32_t backoff_wakeups(const uint8_t fail_count)
{
    uint32_t skip = 0;
    for (uint8_t i = 1; i <= fail_count; ++i)
    {
        const uint8_t shift = i < NTP_BACKOFF_MAX_SHIFT ? i : NTP_BACKOFF_MAX_SHIFT;
        skip += 1UL << shift;

        if (i == 0xFF)   // uint8_t: i++ на 255 ушёл бы в вечный цикл
            break;
    }
    return skip;
}

bool need_ntp_sync(const time_t last_sync, const uint16_t wakeups_since_sync,
                   const uint8_t fail_count, const uint16_t wakeup_per_min,
                   const uint8_t sync_count)
{
    uint32_t deadline = 0;

    if (is_valid_time(last_sync))
    {
        // На прогреве синхронизируемся каждое пробуждение, пока не наберётся
        // пара для измерения поправки. Дальше — раз в неделю.
        deadline = sync_count < NTP_WARMUP_SYNCS ? 1 : sync_deadline_wakeups(wakeup_per_min);
    }
    // Время неизвестно — расписание построить не на чем, срок нулевой.
    // Отступ после неудач всё равно действует: устройство без интернета
    // с завода не должно опрашивать NTP при каждом пробуждении.

    return (uint32_t)wakeups_since_sync >= deadline + backoff_wakeups(fail_count);
}

time_t estimate_time(const time_t last_sync, const uint16_t wakeups_since_sync,
                     const uint16_t wakeup_per_min)
{
    if (!is_valid_time(last_sync))
        return 0;

    return last_sync + (time_t)wakeups_since_sync * wakeup_per_min * 60;
}
