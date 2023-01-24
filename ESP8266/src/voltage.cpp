#include "voltage.h"

Voltage::Voltage() {}

Voltage::~Voltage() {}

void Voltage::begin()
{
    voltage = ESP.getVcc();
    minimum = voltage;
    maximum = voltage;
}

/**
 * @brief Измеряет напряжение на системной шине в миливвольтах
 *
 */
void Voltage::update()
{
    voltage = ESP.getVcc();
    if (voltage < minimum)
        minimum = voltage;
    if (voltage > maximum)
        maximum = voltage;
}
/**
 * @brief Разница между измеренными напряжениями  в миливольтах
 *
 * @return Напряжение в миливвольтах
 */
uint16_t Voltage::diff()
{
    return maximum - minimum;
}
/**
 * @brief Возвращает занчение последнего измеренного напряжение на системной шине в миливвольтах
 *
 * @return Напряжение в миливвольтах
 */
uint16_t Voltage::value()
{
    return voltage;
}
/**
 * @brief Возвращает признак, что батарейки требуют замены
 *
 * @return true  если разница измеренных напряжений больше 100 мВ и напряжение меньше 2.9В, (батарейка разрядилась)
 * @return false  если разница измеренных напряжений меньше 100 мВ и напряжение больше 2.9В, (батарейка заряжена)
 */
bool Voltage::low_voltage()
{
    // если просдка больше 100 мВ и напряжение меньше 2.9В
    return (diff() >= ALERT_POWER_DIFF_MV) && (voltage < BATTERY_LOW_THRESHOLD_MV);
}

/**
 * @brief Определяет уровень батареи в процентах
 *
 * @return уровень батареи в процентах.
 */
uint8_t Voltage::get_battery_level()
{
    // https://mysku.club/blog/diy/81535.html
    //  поведение бтареек и аккумуляторов при разрядке

    uint8_t percent = 100;

    if (low_voltage()) // все измерения в миливольтах!
    {
        // если разница больше 100 мВ и напряжение меньше 2.9В то батарейки разрядились
        percent = 0;
    }
    else
    {
        if (diff() > 50)
        {
            // батарейка начала разряжаться
            percent = int(round((100.0 - diff()) * 0.5));
        }
        else
        {
            // батарейка заряжена
            percent = int(25 + round((50.0 - diff()) * 1.5));
        }
    }

    return percent;
}
