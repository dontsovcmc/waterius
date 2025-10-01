#pragma once

#include "soc/sens_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_periph.h"
#include "soc/rtc_io_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#define LED_S2			(gpio_num_t)15
#define LED_STATE		GPIO_NUM_35
#define BATT_VOL		GPIO_NUM_9
#define BATT_VOL_ADC	ADC_CHANNEL_8
#define BATT_EN			GPIO_NUM_12

enum class power_t {
    Battery         = 0,
    USB             = 1,
};

enum class ulp_event_t {
    NONE            = 0,
    TIME            = 1,
    BUTTON_SHORT    = 2,
    BUTTON_LONG    	= 3,
    USB             = 4,
};

struct ulp_config_t {
    bool            use_led;
    bool            use_out;
    unsigned int	debounce_max_count;
};

enum class ulp_channel_type_t {
    Disabled        = 0,
    Discrete        = 1,
    Analog          = 2,
};

struct ulp_channel_t {
    uint16_t        type;
    uint16_t        pulse_count;
    unsigned int	adc_value;
};

struct board_data_t {
    uint8_t version;    				// Версия ПО
    ulp_config_t    config;				// Настройка режимов ULP
    ulp_channel_t   ch0;
    ulp_channel_t   ch1;
	uint32_t		impulses0;
	uint32_t		impulses1;
	power_t			power;
	bool			usb_connected;
    unsigned int    battery_voltage;
    unsigned int	wake_up_counter;
    unsigned int	wake_up_period;
	unsigned int	button_time;
	unsigned int	input;				// Входа GPIO, обработываемые ulp
	bool			read();
	bool			set_counter_type_0(const uint8_t type0);
	bool			set_counter_type_1(const uint8_t type1);
};

struct SlaveData
{
    // Header
    uint8_t version;    // Версия ПО Attiny
    uint8_t service;    // Причина загрузки Attiny
    uint16_t reserved4; // Напряжение питания в мВ (после включения wi-fi под нагрузкой )
    uint8_t reserved;
    uint8_t setup_started_counter;
    uint8_t resets;
    uint8_t model;         // WATERIUS_CLASSIC или  WATERIUS_4C2W
    uint8_t counter_type0; // Тип входа, вход 0
    uint8_t counter_type1; //           вход 1
    uint32_t impulses0;    // Импульсов, канал 0
    uint32_t impulses1;    //           канал 1
    uint16_t adc0;         // Уровень,   канал 0
    uint16_t adc1;         //           канал 1

    // HEADER_DATA_SIZE

    uint8_t crc = 0; // Всегда в конце структуры данных
    uint8_t reserved2 = 0;
    uint8_t reserved3 = 0;
    uint8_t reserved5 = 0;
    // Кратно 16bit https://github.com/esp8266/Arduino/issues/1825
};

extern ulp_event_t ulp_event;
extern board_data_t board;

ulp_event_t get_wakeup_event(void);
void deep_sleep(void);
void initialize_pins(void);
void initialize_rtc_pins(void);
void init_ulp_program(void);
size_t autoprint(const char *format, ...);

