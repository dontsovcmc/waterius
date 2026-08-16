#include "readings.h"

/*
Берем начальные показания и кол-во импульсов,
вычисляем текущие показания по новому кол-ву импульсов
*/
ReadingsStatus calc_readings(Settings &sett, const AttinyData &data, CalculatedData &cdata)
{
    ReadingsStatus status;

    if (sett.factor0 > 0)
    {
        if (data.impulses0 < sett.impulses0_start) {
            sett.impulses0_start = data.impulses0;
            // Лучше потеряем в точности, чем будет показания миллионы
            status.impulses0_start_reset = true;
        }

        if (sett.counter0_name == CounterName::ELECTRO)
        {
            // factor0 кол-во импульсов на 1 кВт * ч
            cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / (sett.factor0 * 1.0);
            cdata.delta0 = (data.impulses0 - sett.impulses0_previous) / (sett.factor0 * 1.0);
        }
        else
        {
            // factor0 кол-во литров на 1 импульс, переводим в кубометры
            cdata.channel0 = sett.channel0_start + (data.impulses0 - sett.impulses0_start) / 1000.0 * sett.factor0;
            cdata.delta0 = (data.impulses0 - sett.impulses0_previous) * sett.factor0;
        }
    }

    if (sett.factor1 > 0)
    {
        if (data.impulses1 < sett.impulses1_start) {
            sett.impulses1_start = data.impulses1;
            status.impulses1_start_reset = true;
        }

        if (sett.counter1_name == CounterName::ELECTRO)
        {
            // factor1 кол-во импульсов на 1 кВт * ч
            cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / (sett.factor1 * 1.0);
            cdata.delta1 = (data.impulses1 - sett.impulses1_previous) / (sett.factor1 * 1.0);
        }
        else
        {
            // factor1 кол-во литров на 1 импульс, переводим в кубометры
            cdata.channel1 = sett.channel1_start + (data.impulses1 - sett.impulses1_start) / 1000.0 * sett.factor1;
            cdata.delta1 = (data.impulses1 - sett.impulses1_previous) * sett.factor1;
        }
    }

    return status;
}

uint16_t get_auto_factor(const uint32_t runtime_impulses,
                         const uint32_t impulses,
                         const uint16_t factor,
                         const uint16_t factor_cold)
{
    switch (factor)
    {
        case AUTO_IMPULSE_FACTOR:
            return (runtime_impulses - impulses <= IMPULS_LIMIT_1) ? 10 : 1;
        case AS_COLD_CHANNEL:
            return factor_cold;
    }
    return factor;
}
