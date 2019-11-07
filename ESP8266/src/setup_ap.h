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

class LongParameter : public WiFiManagerParameter {
public:

    LongParameter(const char *id, const char *placeholder, long value, const uint8_t length = 10)
        : WiFiManagerParameter("") {

        init(id, placeholder, String(value).c_str(), length, " type=\"number\"", WFM_LABEL_BEFORE);
    }

    long getValue() {
        return String(WiFiManagerParameter::getValue()).toInt();
    }
};

class FloatParameter : public WiFiManagerParameter {
public:

    FloatParameter(const char *id, const char *placeholder, float value, const uint8_t length = 10)
        : WiFiManagerParameter("") {
            
        init(id, placeholder, String(value).c_str(), length, " type=\"number\" step=\"0.01\" placeholder=\"0.00\"", WFM_LABEL_BEFORE);
    }

    float getValue() {
        String val(WiFiManagerParameter::getValue());
        val.replace(",", ".");
        return val.toFloat();
    }
};


#endif

