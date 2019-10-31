#include <Arduino.h>
#include "driver/rtc_io.h"

#define LOGLEVEL 6

#include "Logging.h"
#include "wbutton.h"
#include "setup_ap.h"
#include "transmit.h"

#define LED_PIN    GPIO_NUM_4
#define CAMERA_PIN GPIO_NUM_32
#define BUTTON_PIN GPIO_NUM_7

void setup()
{
    
    gpio_pad_select_gpio(GPIO_NUM_12);
    gpio_set_direction(GPIO_NUM_12, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_12, GPIO_FLOATING);

    gpio_pad_select_gpio(GPIO_NUM_14);
    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_14, GPIO_FLOATING);

    for (int i=32; i<40; i++) {
        gpio_pad_select_gpio(i);
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_INPUT);
        if (i < 34) gpio_set_pull_mode((gpio_num_t)i, GPIO_FLOATING);
    }

    esp_sleep_enable_timer_wakeup(60 * 1000000);
    esp_deep_sleep_disable_rom_logging();
    esp_deep_sleep_start();
}

void loop()
{ }