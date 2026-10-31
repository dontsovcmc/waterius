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

// Коды вспышек и их выбор по итогам сеанса — в чистом ядре, под тестами
#include "core/blink.h"

void blynk_error(enum ErrorBlynks code);

void setup_leds();
void release_leds();
uint8_t wait_button_release();

#endif
