#include "voltage.h"

Voltage::Voltage() {}

Voltage::~Voltage() {}

void Voltage::begin()
{
    _voltage = ESP.getVcc();
    _min = _voltage;
    _max = _voltage;
    _indx = 0;
    _values[_indx] = _voltage;
    _indx++;
}

/**
 * @brief Измеряет напряжение на системной шине в миливвольтах
 *
 */
void Voltage::update()
{
    _voltage = ESP.getVcc();
    if (_voltage < _min)
        _min = _voltage;
    if (_voltage > _max)
        _max = _voltage;
    _values[_indx % NUM_PROBES] = _voltage;
    _indx++;
}
/**
 * @brief Разница между измеренными напряжениями  в миливольтах
 *
 * @return Напряжение в миливвольтах
 */
uint16_t Voltage::diff()
{
    return _max - _min;
}
/**
 * @brief Возвращает занчение последнего измеренного напряжение на системной шине в миливвольтах
 *
 * @return Напряжение в миливвольтах
 */
uint16_t Voltage::value()
{
    return _voltage;
}
/**
 * @brief Возвращает признак, что батарейки требуют замены
 *
 * @return true  если разница измеренных напряжений больше 100 мВ и напряжение меньше 2.9В, (батарейка разрядилась)
 * @return false  если разница измеренных напряжений меньше 100 мВ и напряжение больше 2.9В, (батарейка заряжена)
 */
bool Voltage::low_voltage()
{
    // если просдка больше 100 мВ или напряжение меньше 2.9В
    return (diff() >= ALERT_POWER_DIFF_MV) || (_voltage < BATTERY_LOW_THRESHOLD_MV);
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
        if (diff() > LOW_BATTERY_DIFF_MV)
        {
            // батарейка начала разряжаться
            percent = int(round((ALERT_POWER_DIFF_MV - diff()) * 0.5));
        }
        else
        {
            // батарейка заряжена
            percent = int(25 + round((LOW_BATTERY_DIFF_MV - diff()) * 1.5));
        }
    }

    return percent;
}

/**
 * @brief Среднее значение измерений
 *
 * @return uint16_t среднее значение напряжения в миливольтах
 */
uint16_t Voltage::average()
{
    uint32_t sum = 0;
    uint8_t cnt = NUM_PROBES ? _indx >= NUM_PROBES : _indx;
    uint16_t avrg = 0;
    
    for (int i = 0; i < cnt; i++)
    {
        sum += _values[i]; // суммируем
    }
    
    if (cnt > 0)
    {
        avrg = (uint16_t)sum / cnt;
    }
    return avrg;
}