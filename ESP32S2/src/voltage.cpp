#include "voltage.h"
#include "Logging.h"

void Voltage::begin()
{
    _voltage = 0; //TODO ESP.getVcc();
    _min_voltage = _voltage;
    _max_voltage = _voltage;
    _num_probes = 0;
    _probes[_num_probes % MAX_PROBES] = _voltage;
    _num_probes++;
}

/**
 * @brief Измеряет напряжение на системной шине в миливвольтах
 *
 */
void Voltage::update()
{
    _voltage = 0; //TODO ESP.getVcc();
    _min_voltage = _min(_voltage, _min_voltage);
    _max_voltage = _max(_voltage, _max_voltage);
    _probes[_num_probes % MAX_PROBES] = _voltage;
#ifdef DEBUG_VOLTAGE
    LOG_INFO(F("VOLTAGE: Probe #: ") << _num_probes);
    LOG_INFO(F("VOLTAGE: Value (mV):") << _voltage);
    LOG_INFO(F("VOLTAGE: Min (mV):") << _min_voltage);
    LOG_INFO(F("VOLTAGE: Max (mV):") << _max_voltage);
#endif
    _num_probes++;
}
/**
 * @brief Разница между измеренными напряжениями  в миливольтах
 *
 * @return Напряжение в миливвольтах
 */
uint16_t Voltage::diff()
{
    return _max_voltage - _min_voltage;
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
    return (diff() >= ALERT_POWER_DIFF_MV) || (average() < BATTERY_LOW_THRESHOLD_MV);
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
    uint16_t avrg, sum = 0;
    int count = _num_probes > MAX_PROBES ? MAX_PROBES : _num_probes;
    for (int i = 0; i < count; i++)
    {
        sum += _probes[i];
    }
    if (count > 0)
    {
        avrg = sum / count;
    }
    else
    {
        avrg = _voltage;
    }
#ifdef DEBUG_VOLTAGE
    LOG_INFO(F("VOLTAGE: Probes count: ") << _num_probes);
    LOG_INFO(F("VOLTAGE: Value (mV):") << _voltage);
    LOG_INFO(F("VOLTAGE: Min (mV):") << _min_voltage);
    LOG_INFO(F("VOLTAGE: Max (mV):") << _max_voltage);
    LOG_INFO(F("VOLTAGE: Average (mV):") << avrg);
#endif
    return avrg;
}

Voltage *get_voltage()
{
    static Voltage voltage;
    return &voltage;
}
