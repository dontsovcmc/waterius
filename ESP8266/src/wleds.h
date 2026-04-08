/**
 * @file wleds.h
 * @brief Моргание светодиодами при включении и при ошибках в Ватериусе2
 * @version 0.1
 * @date 2025-10-30
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef WLEDS_h_
#define WLEDS_h_

#include <Arduino.h>

enum ErrorBlynks
{
    ERROR_OK = 0,
    ERROR_LOW_VOLTAGE = 1,
    ERROR_CONNECT_ROUTER = 2,
    ERROR_CONNECT_CLOUD = 3,
    ERROR_CONNECT_MQTT = 4,
    ERROR_CONFIG = 5,
    ERROR_CLOUD_ANSWER = 6
};

void blynk_led(uint8_t pin, uint8_t times, uint16_t delay_ms=200, uint16_t pause_ms=400);

void blynk_error(enum ErrorBlynks code);

void setup_leds();
void release_leds();
uint8_t wait_button_release();

#endif
