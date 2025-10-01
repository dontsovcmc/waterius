
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sdkconfig.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "Logging.h"
#include "config.h"
#include "senders/sender_waterius.h"
#include "senders/sender_http.h"
#include "senders/sender_mqtt.h"
#include "portal/active_point.h"
#include "voltage.h"
#include "utils.h"
#include "porting.h"
#include "json.h"
#include "Ticker.h"
#include "sync_time.h"
#include "wifi_helpers.h"
#include "config.h"
#include "board.h"

SlaveData data;       // Данные от Attiny85
Settings sett;        // Настройки соединения и предыдущие показания из EEPROM
CalculatedData cdata; // вычисляемые данные
bool config_loaded = false;
uint8_t mode = 0;

// Debug UART: TX GPIO_40, RX GPIO_38

//=====================================================================================
// Выполняется однократно при включении
//=====================================================================================

void setup()
{
    // Инициализация портов
	USBSerial.begin(115200);        // Консоль приложения
	Serial0.begin(115200);          // Системная консоль
    autoprint("Booted\r\n");
    autoprint("Build: %s %s\r\n", __DATE__, __TIME__);

	// Установка пинов
	initialize_pins();
    gpio_set_level(LED_S2, 1);
    gpio_set_level(LED_STATE, 1);

	// Загружаем конфиг
    config_loaded = load_config(sett);
	if (config_loaded)
    	autoprint("Config loaded\r\n");

    // Определяем причину запуска
    get_wakeup_event();
    if (ulp_event == ulp_event_t::NONE) {
        // Обычный запуск
        initialize_rtc_pins();
        init_ulp_program();
    } else {
        // Проснулись по сигналу от ULP
    }

	// Читаем данные
   	board.read();

    autoprint("Initializing complete\r\n");
}

//=====================================================================================
// Выполняется в цикле после setup
//=====================================================================================

void loop()
{
	static unsigned long interval_1s = 0;
	unsigned long now = millis();
	unsigned long elapsed = now - interval_1s;
	if (elapsed > 1000) 
	{
		interval_1s = now;
		// Читаем данные
    	board.read();
		// Обновляем светодиоды
    	gpio_set_level(LED_STATE, 1);
    	gpio_set_level(LED_S2, (board.power == power_t::USB));
		// Пишем в консоль состояние
  		static const char power_text[][16] = { "Battery", "USB" };
    	static const char usb_text[][16] = { "not connected", "connected" };
    	autoprint("wake %u/%u, power %s, voltage %u, usb %s\r\n", board.wake_up_counter, board.wake_up_period, power_text[(uint)board.power], board.battery_voltage, usb_text[board.usb_connected]);
    	autoprint("pulse %u/%u, adc %u/%u\r\n", board.impulses0, board.impulses1, board.ch0.adc_value, board.ch1.adc_value);
		autoprint("input %u\r\n", board.input);
    	if (board.button_time) autoprint("button %u\r\n", board.button_time);
		update_config(sett);
	}

	if (mode == 0) 
	{
		if (ulp_event == ulp_event_t::TIME)
			mode = TRANSMIT_MODE;
		else if (ulp_event == ulp_event_t::BUTTON_SHORT)
			mode = MANUAL_TRANSMIT_MODE;
		else if (ulp_event == ulp_event_t::BUTTON_LONG)
			mode = SETUP_MODE;
		ulp_event = ulp_event_t::NONE;
	}

    if (mode)
    {
		if (sett.mode == 0)
        {
			// Загружаем конфигурацию из EEPROM
        	config_loaded = load_config(sett);
        	sett.mode = mode;
        	autoprint("Startup mode: %u\r\n", mode);

        	// Вычисляем текущие показания
        	calculate_values(sett, cdata);
		}

        // Режим настройки - запускаем точку доступа на 192.168.4.1
        // Запускаем точку доступа с вебсервером
        if (active_point() == active_point_state_t::Finish)
        {
			mode = 0;
            sett.setup_time = millis();
            sett.setup_finished_counter++;

            autoprint("Finish setup mode...");
            store_config(sett);

            wifi_shutdown();

            autoprint("Restart ESP");
            ESP.restart();

            return; // сюда не должно дойти никогда
        }

/*        if (config_loaded)
        {
            if (wifi_connect(sett))
            {
                log_system_info();

                DynamicJsonDocument json_data(JSON_DYNAMIC_MSG_BUFFER);

#ifndef MQTT_DISABLED
                // Подключаемся и подписываемся на мктт
                if (is_mqtt(sett))
                {
                    connect_and_subscribe_mqtt(sett, data, cdata, json_data);
                }
#endif
                // устанавливать время только при использовани хттпс или мктт
                if (is_mqtt(sett) || is_https(sett.waterius_host) || is_https(sett.http_url))
                {
                    if (!sync_ntp_time(sett)) {
                        sett.ntp_error_counter++;
                    }
                }

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

                // Формироуем JSON
                get_json_data(sett, data, cdata, json_data);

                LOG_INFO(F("Free memory: ") << ESP.getFreeHeap());

#ifndef WATERIUS_RU_DISABLED
                if (send_waterius(sett, json_data))
                {
                    LOG_INFO(F("HTTP: Send OK"));
                }
#endif

#ifndef HTTPS_DISABLED
                if (send_http(sett, json_data))
                {
                    LOG_INFO(F("HTTP: Send OK"));
                }
#endif

#ifndef MQTT_DISABLED
                if (is_mqtt(sett))
                {
                    if (send_mqtt(sett, data, cdata, json_data))
                    {
                        LOG_INFO(F("MQTT: Send OK"));
                    }
                }
                else
                {
                    LOG_INFO(F("MQTT: SKIP"));
                }
#endif
                // Все уже отправили,  wifi не нужен - выключаем
                //wifi_shutdown();

                update_config(sett);

                if (!masterI2C.setWakeUpPeriod(sett.set_wakeup))
                {
                    LOG_ERROR(F("Wakeup period wasn't set"));
                }
                else // Разбуди меня через...
                {
                    LOG_INFO(F("Wakeup period, min:") << sett.wakeup_per_min);
                    LOG_INFO(F("Wakeup period (adjusted), min:") << sett.set_wakeup);
                }

                store_config(sett);
            }
        }*/
		mode = 0;
    } 

    if (!config_loaded)
    {
        delay(500);
        blink_led(3, 1000, 500);
    }

	if (mode == 0)
	{
		// Если задач нету
		if (board.power == power_t::Battery)
		{
			// При питании от батареи - уходим в сон
    		gpio_set_level(LED_STATE, 0);
    		//deep_sleep();
		}
		else
		{
			// При питании от USB - продолжаем работать
		}
	}
	delay(100);
}