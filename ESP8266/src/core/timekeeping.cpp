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
