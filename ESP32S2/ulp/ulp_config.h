#pragma once

/* Ints are used here to be able to include the file in assembly as well */

#define ULP_BATTERY_IO          9   // Battery voltage GPIO number
#define ULP_BATTERY_ADC         8   // ADC_CHANNEL_8, GPIO9 on ESP32-S2
#define ULP_BATT_EN_IO          12  // Battery enable GPIO number
#define ULP_BUTTON_IO			11  // Button GPIO number

#define ULP_PULL_UP             8   // Pull-up GPIO number
#define ULP_PWR             	7   // Power GPIO number

#define ULP_CH0_IO              3   // CH0 GPIO number
#define ULP_CH0_ADC_CHANNEL     2   // ADC_CHANNEL_2, GPIO3 on ESP32-S2
#define ULP_CH0_LED             10  // CH0 LED GPIO number
#define ULP_CH0_OUT             2	// CH0 Output relay&LED GPIO number

#define ULP_CH1_IO              5   // CH1 GPIO number
#define ULP_CH1_ADC_CHANNEL     4   // ADC_CHANNEL_6, GPIO7 on ESP32-S2
#define ULP_CH1_LED             6	// CH1 LED GPIO number
#define ULP_CH1_OUT             1	// CH1 Output relay&LED GPIO number

#define ULP_WAKEUP_PERIOD       100000  // ULP wake up period 100ms
#define ULP_WAKEUP_PERIOD_SEC	(1000000 / ULP_WAKEUP_PERIOD)
#define ULP_BEBOUNCE_MAX_COUNT  3       // Value to which debounce_counter gets rese

#define ULP_ADC_ATTEN			ADC_ATTEN_DB_6
#define ULP_ADC_AREF			(1500 * 4)
#define ULP_ADC_MAX				0x1FFF
#define ULP_ADC_VOLTAGE(x)      ((x) * ULP_ADC_AREF / ULP_ADC_MAX)
#define ULP_BATTERY_R_HIGH		470
#define ULP_BATTERY_R_LOW		160
#define ULP_BATTERY_RATIO       ((ULP_BATTERY_R_HIGH + ULP_BATTERY_R_LOW) / ULP_BATTERY_R_LOW)

/*
RTC-GPIO 0-14, 19-21

ADC input channels have a 12 bit resolution. This means that you can get analogue readings ranging from 0 to 4095, 
in which 0 corresponds to 0V and 4095 to 3.3V.

GPIO1 ADC1_CH0
GPIO2 ADC1_CH1
GPIO3 ADC1_CH2
GPIO4 ADC1_CH3
GPIO5 ADC1_CH4
GPIO6 ADC1_CH5
GPIO7 ADC1_CH6
GPIO8 ADC1_CH7
GPIO9 ADC1_CH8
GPIO10 ADC1_CH9

ADC2 pins cannot be used when Wi-Fi is used. 
*/