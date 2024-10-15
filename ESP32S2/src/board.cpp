#include "board.h"

#include "esp32s2/ulp.h"
#include "esp_sleep.h"
//#include "esp32/ulp_adc.h"
#include "ulp_main.h"
#include "../ulp/ulp_config.h"

//#include "tinyusb.h"
//#include "tusb_cdc_acm.h"
//#include "tusb_console.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

//=====================================================================================
void initialize_pins(void)
{
    gpio_set_direction(LED_S2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_STATE, GPIO_MODE_OUTPUT);
    gpio_set_direction(BATT_VOL, GPIO_MODE_INPUT);

    gpio_set_direction(BATT_EN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(BATT_EN);
    gpio_pullup_en(BATT_EN);
    gpio_hold_en(BATT_EN);
}

//=====================================================================================
void initialize_rtc_pins(void)
{
	// ULP_BUTTON_IO: input, pull-up
    rtc_gpio_init((gpio_num_t)ULP_BUTTON_IO);
    rtc_gpio_set_direction((gpio_num_t)ULP_BUTTON_IO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_BUTTON_IO);
    rtc_gpio_pullup_en((gpio_num_t)ULP_BUTTON_IO);
    rtc_gpio_hold_en((gpio_num_t)ULP_BUTTON_IO);

	// ULP_PULL_UP: output, no pull-up
    rtc_gpio_init((gpio_num_t)ULP_PULL_UP);
    rtc_gpio_set_direction((gpio_num_t)ULP_PULL_UP, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_PULL_UP);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_PULL_UP);
    rtc_gpio_hold_en((gpio_num_t)ULP_PULL_UP);

	// ULP_PWR: output, no pull-up
    rtc_gpio_init((gpio_num_t)ULP_PWR);
    rtc_gpio_set_direction((gpio_num_t)ULP_PWR, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_PWR);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_PWR);
    rtc_gpio_hold_en((gpio_num_t)ULP_PWR);

	// ULP_CH0_IO: input, no pull-up
    rtc_gpio_init((gpio_num_t)ULP_CH0_IO);
    rtc_gpio_set_direction((gpio_num_t)ULP_CH0_IO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_CH0_IO);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_CH0_IO);
    rtc_gpio_hold_en((gpio_num_t)ULP_CH0_IO);

	// ULP_CH1_IO: input, no pull-up
    rtc_gpio_init((gpio_num_t)ULP_CH1_IO);
    rtc_gpio_set_direction((gpio_num_t)ULP_CH1_IO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_CH1_IO);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_CH1_IO);
    rtc_gpio_hold_en((gpio_num_t)ULP_CH1_IO);

	// ULP_CH0_LED: output, no pull-up
    rtc_gpio_init((gpio_num_t)ULP_CH0_LED);
    rtc_gpio_set_direction((gpio_num_t)ULP_CH0_LED, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_CH0_LED);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_CH0_LED);
    rtc_gpio_hold_en((gpio_num_t)ULP_CH0_LED);

	// ULP_CH1_LED: output, no pull-up
    rtc_gpio_init((gpio_num_t)ULP_CH1_LED);
    rtc_gpio_set_direction((gpio_num_t)ULP_CH1_LED, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_CH1_LED);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_CH1_LED);
    rtc_gpio_hold_en((gpio_num_t)ULP_CH1_LED);

    rtc_gpio_init((gpio_num_t)ULP_CH0_OUT);
    rtc_gpio_set_direction((gpio_num_t)ULP_CH0_OUT, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_CH0_OUT);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_CH0_OUT);
    rtc_gpio_hold_en((gpio_num_t)ULP_CH0_OUT);

    rtc_gpio_init((gpio_num_t)ULP_CH1_OUT);
    rtc_gpio_set_direction((gpio_num_t)ULP_CH1_OUT, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)ULP_CH1_OUT);
    rtc_gpio_pullup_dis((gpio_num_t)ULP_CH1_OUT);
    rtc_gpio_hold_en((gpio_num_t)ULP_CH1_OUT);
}

//=====================================================================================
void init_ulp_program(void)
{
    // Load binary
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    //ESP_ERROR_CHECK(err);

    /* Initialize some variables used by ULP program.
     * Each 'ulp_xyz' variable corresponds to 'xyz' variable in the ULP program.
     * These variables are declared in an auto generated header file,
     * 'ulp_main.h', name of this file is defined in component.mk as ULP_APP_NAME.
     * These variables are located in RTC_SLOW_MEM and can be accessed both by the
     * ULP and the main CPUs.
     *
     * Note that the ULP reads only the lower 16 bits of these variables.
     */
    /*ulp_wake_up_counter = 0;
    ulp_wake_up_period = 120 * ULP_WAKEUP_PERIOD_SEC;

    ulp_ch0_type = 1;
    ulp_ch1_type = 2;
	ulp_use_led = 1;
	ulp_use_out = 1;*/

	// Init ADC, 13 bit, voltage divider set to 2 times
    /*ulp_adc_cfg_t cfg_1 = {
        .adc_n    = ADC_UNIT_1,
        .channel  = ULP_BATTERY_ADC,
        .width    = ADC_BITWIDTH_13,
        .atten    = ULP_ADC_ATTEN,
        .ulp_mode = ADC_ULP_MODE_FSM,
    };*/
    //ESP_ERROR_CHECK(ulp_adc_init(&cfg_1));

#if CONFIG_IDF_TARGET_ESP32
    /* Disconnect GPIO12 and GPIO15 to remove current drain through
     * pullup/pulldown resistors on modules which have these (e.g. ESP32-WROVER)
     * GPIO12 may be pulled high to select flash voltage.
     */
    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);
#endif // CONFIG_IDF_TARGET_ESP32

    // Suppress boot messages
    esp_deep_sleep_disable_rom_logging(); 

    // Set ULP wake up period
    ulp_set_wakeup_period(0, ULP_WAKEUP_PERIOD);

    // Start the program
    //err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    //ESP_ERROR_CHECK(err);
}

//=====================================================================================
void initialize_usb_serial(void)
{
    /*const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
    };
    if (tinyusb_driver_install(&tusb_cfg) == ESP_OK) {

        tinyusb_config_cdcacm_t acm_cfg = {
            .usb_dev = TINYUSB_USBDEV_0,
            .cdc_port = TINYUSB_CDC_ACM_0,
            .rx_unread_buf_sz = 64,
            .callback_rx = NULL,
            .callback_rx_wanted_char = NULL,
            .callback_line_state_changed = NULL,
            .callback_line_coding_changed = NULL
        }; 
        tusb_cdc_acm_init(&acm_cfg);
    }
    
    esp_tusb_init_console(TINYUSB_CDC_ACM_0);*/
}
