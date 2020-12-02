#ifndef _WATERIUS_VOLTAGE_h
#define _WATERIUS_VOLTAGE_h

#include "setup.h"
#include "Logging.h"
#include "master_i2c.h"

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