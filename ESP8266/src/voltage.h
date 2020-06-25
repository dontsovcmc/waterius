#ifndef _WATERIUS_VOLTAGE_h
#define _WATERIUS_VOLTAGE_h

#include "setup.h"
#include "Logging.h"
#include "master_i2c.h"

#ifdef BUILD_WATERIUS_4C2W

#define LOW_VOLTAGE 3300
#define CRITICAL_VOLTAGE 3200

uint32_t measure_voltage()
{
    uint32_t level = analogRead(A0);
    LOG_INFO(FPSTR(S_ESP), F("A0 level: ") << level);
    return 3.0 * level / 1023 * 10.0 / 3.0;   //100k + 30k resistors
}   


bool check_voltage(SlaveData &data, CalculatedData &cdata)
{
    uint32_t prev = data.voltage;
    
    data.voltage = measure_voltage();
    uint32_t diff = abs(prev - data.voltage);
    if (diff > cdata.voltage_diff) {
        cdata.voltage_diff = diff;
    }
    
    cdata.low_voltage = data.voltage < LOW_VOLTAGE;
    return data.voltage < CRITICAL_VOLTAGE;
}

#else

#define LOW_BATTERY_DIFF_MV 50  //надо еще учесть качество замеров (компаратора у attiny)
#define ALERT_POWER_DIFF_MV 100

extern MasterI2C masterI2C;

bool check_voltage(SlaveData &data, CalculatedData &cdata)
{   
    uint32_t prev = data.voltage;
	if (masterI2C.getSlaveData(data)) {
        uint32_t diff = abs(prev - data.voltage);
        if (diff > cdata.voltage_diff) {
            cdata.voltage_diff = diff;
        }
        
        cdata.low_voltage = cdata.voltage_diff >= LOW_BATTERY_DIFF_MV;
        return cdata.voltage_diff < ALERT_POWER_DIFF_MV;
	}
    return true; //пропустим если ошибка i2c
}
#endif

#endif