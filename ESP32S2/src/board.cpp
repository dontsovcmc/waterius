#include "board.h"

#include "esp32s2/ulp.h"
#include "ulp_adc.h"
#include "esp_sleep.h"
#include "Logging.h"
#include "ulp_main.h"
#include "../ulp/ulp_config.h"
#include "config.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

ulp_event_t ulp_event = ulp_event_t::NONE;		// Причина запуска
board_data_t board;
static const char wakeup_text[][16] = { "Undefined", "All", "Ext0", "Ext1", "Timer", "Touchpad", "ULP", "GPIO", "UART", "WiFi", "CoCPU int", "CoCPU crash", "BT" };
static const char event_text[][16] = { "None", "Time", "Button short", "Button long", "USB" };
static const char input_text[][16] = { "Disabled", "Discrete", "Analog" };

//=====================================================================================
ulp_event_t get_wakeup_event(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    autoprint("Startup, cause %s\r\n", wakeup_text[cause]);    

    ulp_event = ulp_event_t::NONE;
    if (cause == ESP_SLEEP_WAKEUP_ULP) {
        ulp_event = (ulp_event_t)ulp_wake_up_event;
        autoprint("ULP wakeup, event: %s\r\n", event_text[(uint)ulp_event]);
    }
    ulp_wake_up_event = 0;
    return ulp_event;
}

//=====================================================================================
void deep_sleep(void)
{
    autoprint("Entering deep sleep\r\n");    
	ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    delay(100);
    esp_deep_sleep_start();    
}

//=====================================================================================
void initialize_pins(void)
{
    autoprint("Initializing pins\r\n");

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
    autoprint("Initializing RTC pins\r\n");

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
void ulp_irq(void *arg)
{
    ulp_event = (ulp_event_t)ulp_wake_up_event;
    ulp_wake_up_event = 0;
    autoprint("Interrupt from ULP: %s\r\n", event_text[(uint)ulp_event]);
}

//=====================================================================================
void init_ulp_program(void)
{
    autoprint("Initializing ULP\r\n");
    // Загружаем двоичный файл программы ULP-ядра
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

    /* Initialize some variables used by ULP program.
     * Each 'ulp_xyz' variable corresponds to 'xyz' variable in the ULP program.
     * These variables are declared in an auto generated header file,
     * 'ulp_main.h', name of this file is defined in component.mk as ULP_APP_NAME.
     * These variables are located in RTC_SLOW_MEM and can be accessed both by the
     * ULP and the main CPUs.
     *
     * Note that the ULP reads only the lower 16 bits of these variables.
     */
    
    ulp_wake_up_counter = 0;
    ulp_wake_up_period = 120 * ULP_WAKEUP_PERIOD_SEC;

	board.set_counter_type_0(sett.counter_type0);
    board.ch0.pulse_count = sett.impulses0_previous & UINT16_MAX;
	ulp_ch0_pulse_count = board.ch0.pulse_count;
   	autoprint("Counter 0: %s, %lu\r\n", input_text[board.ch0.type], sett.impulses0_previous);

	board.set_counter_type_1(sett.counter_type1);
    board.ch1.pulse_count = sett.impulses1_previous & UINT16_MAX;
	ulp_ch1_pulse_count = board.ch1.pulse_count;
   	autoprint("Counter 1: %s, %lu\r\n", input_text[board.ch1.type], sett.impulses1_previous);

	ulp_use_led = 1;
	ulp_use_out = 1;

	// Init ADC, 13 bit, voltage divider set to 2 times
    ulp_adc_cfg_t cfg_1 = {
        .adc_n    = ADC_UNIT_1,
        .channel  = (adc_channel_t)ULP_BATTERY_ADC,
        .atten    = ULP_ADC_ATTEN,
        .width    = ADC_BITWIDTH_13,
        .ulp_mode = ADC_ULP_MODE_FSM,
    };
    ESP_ERROR_CHECK(ulp_adc_init(&cfg_1));

#if CONFIG_IDF_TARGET_ESP32
    /* Disconnect GPIO12 and GPIO15 to remove current drain through
     * pullup/pulldown resistors on modules which have these (e.g. ESP32-WROVER)
     * GPIO12 may be pulled high to select flash voltage.
     */
    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);
#endif // CONFIG_IDF_TARGET_ESP32

    // Отключаем сообщения загрузчика
    esp_deep_sleep_disable_rom_logging(); 

    // Задаем период запуска программы ULP
    ulp_set_wakeup_period(0, ULP_WAKEUP_PERIOD);

    // Запускаем ее
    err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_ERROR_CHECK(err);

	// Устанавливаем обработчик прерывания
	ulp_isr_register(ulp_irq, nullptr);
}

//=====================================================================================
bool board_data_t::read()
{
	version = 25;

    config.use_led = ulp_use_led ? true : false;
    config.use_out = ulp_use_out ? true : false;
    config.debounce_max_count = ULP_BEBOUNCE_MAX_COUNT;
    
    ch0.type = ulp_ch0_type & UINT16_MAX;
    ch0.pulse_count = ulp_ch0_pulse_count & UINT16_MAX;
    ch0.adc_value = ulp_ch0_adc_value & UINT16_MAX;
	if ((sett.impulses0_previous & UINT16_MAX) > ch0.pulse_count) {
		impulses0 = (((sett.impulses0_previous >> 16) + 1) << 16) + ch0.pulse_count;
	} else {
		impulses0 = (sett.impulses0_previous & ((uint32_t)UINT16_MAX << 16)) + ch0.pulse_count;
	}

    ch1.type = ulp_ch1_type & UINT16_MAX;
    ch1.pulse_count = ulp_ch1_pulse_count & UINT16_MAX;
    ch1.adc_value = ulp_ch1_adc_value & UINT16_MAX;
	if ((sett.impulses1_previous & UINT16_MAX) > ch1.pulse_count) {
		impulses1 = (((sett.impulses1_previous >> 16) + 1) << 16) + ch1.pulse_count;
	} else {
		impulses1 = (sett.impulses1_previous & ((uint32_t)UINT16_MAX << 16)) + ch1.pulse_count;
	}

	power = gpio_get_level(BATT_EN) ? power_t::Battery : power_t::USB;
    usb_connected = USBSerial;
    battery_voltage = ULP_ADC_VOLTAGE(ulp_battery_adc_value & UINT16_MAX);
    wake_up_counter = ulp_wake_up_counter & UINT16_MAX;
    wake_up_period = ulp_wake_up_period & UINT16_MAX;
	button_time = ulp_button_counter & UINT16_MAX;
	input = ulp_input & UINT16_MAX;

	return true;
}

//=====================================================================================
bool board_data_t::set_counter_type_0(const uint8_t type0)
{
	bool result = true;

	if (type0 == NAMUR) {
		ch0.type = 2;
	} else if ((type0 == DISCRETE) || (type0 == HALL)) {
		ch0.type = 1;
	} else if (type0 == NONE) {
		ch0.type = 0;
	} else {
		ch0.type = 0;
		result = false;
	}
	ulp_ch0_type = ch0.type;

	return result;
}

//=====================================================================================
bool board_data_t::set_counter_type_1(const uint8_t type1)
{
	bool result = true;

	if (type1 == NAMUR) {
		ch1.type = 2;
	} else if ((type1 == DISCRETE) || (type1 == HALL)) {
		ch1.type = 1;
	} else if (type1 == NONE) {
		ch1.type = 0;
	} else {
		ch1.type = 0;
		result = false;
	}
	ulp_ch1_type = ch1.type;

	return result;
}

//=====================================================================================
size_t autoprint(const char *format, ...)
{
	va_list arg;
	size_t result = 0;
	va_start(arg, format);
	bool usb_connected = USBSerial;
	if (usb_connected) 		
		result = USBSerial.vprintf(format, arg);
	else
		result = Serial0.vprintf(format, arg);
  	va_end(arg);
  	return result;
}

//=====================================================================================
#ifdef __cplusplus
extern "C" {
#endif
int board_usb_get_serial(char* buf, int length)
{
	return snprintf(buf, length, "1234");
}
#ifdef __cplusplus
}
#endif