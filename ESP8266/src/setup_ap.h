#ifndef _SETUPAP_h
#define _SETUPAP_h

#include "setup.h"
#include "master_i2c.h"
#include <Arduino.h>

#include <WiFiManager.h>    

/*
Запускаем вебсервер для настройки подключения к Интернету и ввода текущих показаний
*/
void setup_ap(Settings &sett, const SlaveData &data, const CalculatedData &cdata);

/*
Дополнение к WifiManager: классы упрощающие работу
*/

const char LONG_ATTR[] = " type=\"number\"";

class LongParameter : public WiFiManagerParameter {
public:

    LongParameter(const char *id, const char *placeholder, long value, const uint8_t length = 10, const char *custom = LONG_ATTR)
        : WiFiManagerParameter("") {

        init(id, placeholder, String(value).c_str(), length, custom, WFM_LABEL_BEFORE);
    }

    long getValue() {
        return String(WiFiManagerParameter::getValue()).toInt();
    }
};

const char FLOAT_ATTR[] = " type=\"number\" step=\"0.001\"";

class FloatParameter : public WiFiManagerParameter {
public:

    FloatParameter(const char *id, const char *placeholder, float value, const uint8_t length = 10)
        : WiFiManagerParameter("") {
            
        init(id, placeholder, String(value).c_str(), length, FLOAT_ATTR, WFM_LABEL_BEFORE);
    }

    float getValue() {
        String val(WiFiManagerParameter::getValue());
        val.replace(",", ".");
        return val.toFloat();
    }
};


#endif

