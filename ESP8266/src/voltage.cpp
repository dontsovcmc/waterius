#include "voltage.h"
#include "Logging.h"
#include "master_i2c.h"
#include "setup.h"

extern MasterI2C masterI2C;
extern AttinyData runtime_data;
extern Settings sett;

Voltage::Voltage()
{
    _min_voltage = 0xFFFF;
    _max_voltage = 0;
    _num_probes = 0;
}

/**
 * @brief Измеряет напряжение на системной шине в миливвольтах
 *
 */
void Voltage::update()
{
#if WATERIUS_MODEL == WATERIUS_MODEL_1
    _voltage = (uint32_t)ESP.getVcc() * 1000 / 1024;  // system_get_vdd33
#endif
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    masterI2C.updateVoltage();
    delay(5);
    if (!masterI2C.getAttinyData(runtime_data)) {
        return;
    }

    _voltage = (uint32_t)runtime_data.voltage * sett.voltage_cal / 100;
#endif
    if (_min_voltage == 0) {
        _min_voltage = _voltage;
    }
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
 */
bool Voltage::low_voltage()
{
    return ::low_voltage(average(), diff());
}

/**
 * @brief Определяет уровень батареи в процентах
 *
 * @return уровень батареи в процентах.
 */
uint8_t Voltage::get_battery_level()
{
    return ::battery_level(average(), diff());
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
