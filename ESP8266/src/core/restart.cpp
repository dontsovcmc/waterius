#include "restart.h"
#include "types.h"

bool esp_restarted(const uint8_t reset_reason,
                   const uint8_t attiny_version,
                   const uint8_t attiny_flags)
{
    // ЕСП упала сама. Питание при этом не снималось: причина ресета живёт
    // в RTC-памяти, а её обнуляет только пропажа питания.
    if (reset_reason == RST_WDT
        || reset_reason == RST_EXCEPTION
        || reset_reason == RST_SOFT_WDT)
    {
        return true;
    }

    // Старая прошивка attiny шлёт в этом байте резерв, а не флаги
    if (attiny_version < ATTINY_VER_POWER_FLAGS)
    {
        return false;
    }

    return (attiny_flags & ATTINY_FLAG_ESP_POWERED_LONG) != 0;
}
