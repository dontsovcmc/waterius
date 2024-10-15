#pragma once

#include "soc/sens_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_periph.h"
#include "soc/rtc_io_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"

#define LED_S2			(gpio_num_t)15
#define LED_STATE		GPIO_NUM_35
#define BATT_VOL		GPIO_NUM_9
#define BATT_VOL_ADC	ADC_CHANNEL_8
#define BATT_EN			GPIO_NUM_12

void initialize_pins(void);
void initialize_rtc_pins(void);
void init_ulp_program(void);
void initialize_usb_serial(void);
