
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "soc/sens_reg.h"
#include "ulp_config.h"

	.set ch0_gpio, ULP_CH0_IO
	.set ch0_adc, ULP_CH0_ADC_CHANNEL
	.set ch0_led, ULP_CH0_LED
	.set ch0_out, ULP_CH0_OUT
	
	.set ch1_gpio, ULP_CH1_IO
	.set ch1_adc, ULP_CH1_ADC_CHANNEL
	.set ch1_led, ULP_CH1_LED
	.set ch1_out, ULP_CH1_OUT

	.set battery_adc, ULP_BATTERY_ADC

	.set debounce_max_count, ULP_BEBOUNCE_MAX_COUNT
	.set gpio_mask, (1U << ULP_CH0_IO) | (1U << ULP_CH1_IO) | (1U << ULP_BUTTON_IO) | (1U << ULP_BATT_EN_IO)

	.set adc_low_thr, (2000 * ULP_ADC_MAX / 3300)

	// Define variables, which go into .bss section (zero-initialized data)
	.bss
	
	.global local
ch0_next_edge:				// Next input signal edge expected: 0 (negative) or 1 (positive)
	.long 0

ch1_next_edge:				// Next input signal edge expected: 0 (negative) or 1 (positive)
	.long 0

ch0_debounce_counter:		// Counter started when signal value changes. Edge is "debounced" when the counter reaches zero.
	.long 0

ch1_debounce_counter:		// Counter started when signal value changes. Edge is "debounced" when the counter reaches zero.
	.long 0

	.global ch0_pulse_count
ch0_pulse_count:			// Total number of signal edges acquired
	.long 0

	.global ch1_pulse_count
ch1_pulse_count:			// Total number of signal edges acquired
	.long 0

	.global ch0_type
ch0_type:					// Type of counter: 0=none, 1=discrete, 2=Namur
	.long 0

	.global ch1_type
ch1_type:					// Type of counter: 0=none, 1=discrete, 2=Namur
	.long 0

	.global ch0_adc_value
ch0_adc_value:				// ADC value of gpio pin
	.long 0

	.global ch1_adc_value
ch1_adc_value:				// ADC value of gpio pin
	.long 0

	.global battery_adc_value
battery_adc_value:			// ADC value of battery voltage pin
	.long 0

	.global use_led
use_led:					// Use LED to indicate input GPIO state
	.long 0

	.global use_out
use_out:					// Use output to translate input imulses
	.long 0

	.global input
input:						// Input register discrete value
	.long 0

//=====================================================================================
	// Code goes into .text section
	.text
	.global entry
entry:

#if CONFIG_IDF_TARGET_ESP32S2
    /* ESP32S2 powers down RTC periph when entering deep sleep and thus by association SENS_SAR_IO_MUX_CONF_REG */
	WRITE_RTC_FIELD(SENS_SAR_IO_MUX_CONF_REG, SENS_IOMUX_CLK_GATE_EN, 1)
#elif CONFIG_IDF_TARGET_ESP32S3
    /* ESP32S3 powers down RTC periph when entering deep sleep and thus by association SENS_SAR_PERI_CLK_GATE_CONF_REG */
    WRITE_RTC_FIELD(SENS_SAR_PERI_CLK_GATE_CONF_REG, SENS_IOMUX_CLK_EN, 1);
#endif

	// Set GPIO pin ULP_PULL_UP, unhold it, setup to output
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_PULL_UP, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_PULL_UP*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	//WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG + ULP_PULL_UP*4, 16, 16, 0x5208)
	//WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG + ULP_PULL_UP*4, 16, 16, 0x4208)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + ULP_PULL_UP, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + ULP_PULL_UP, 1, 1)

	// Read battery voltage
	adc r1, 0, battery_adc + 1			// R1 = measured battery pin voltage
	move r2, battery_adc_value			// R2 = @ch0_adc_value
	st r1, r2, 0						// battery_adc_value = R1

	// Read the value of lower 16 RTC IOs into R0
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
	move r2, input						// R2 = @input
	and r0, r0, gpio_mask				// R0 = R0 & gpio_mask
	st r0, r2, 0						// input = R0

	jump ch0_read

//=====================================================================================
	.global ch0_read
ch0_read:
	move r0, ch0_type					// R0 = @ch0_type
	ld r0, r0, 0						// R0 = ch0_type
	jumpr ch0_end, 0, EQ				// Jump if R0==0
	jumpr ch0_analog_read, 2, EQ		// Jump if R0==2

ch0_discrete_read:
	move r2, input						// R2 = @input
	ld r0, r2, 0						// R0 = input
	move r3, ch0_gpio					// R3 = ch0_gpio
	rsh r0, r0, r3						// R0 = input >> ch0_gpio
	and r0, r0, 1						// R0 = R0 & 0x0001
	jump ch0_check

ch0_analog_read:
	adc r1, 0, ch0_adc + 1				// R1 = measured gpio pin voltage
	move r2, ch0_adc_value				// R2 = @ch0_adc_value
	st r1, r2, 0						// ch0_adc_value = R1

	// compare with adc_low_thr
	move r0, 1							// R0 = 1
	move r3, adc_low_thr				// R3 = adc_low_thr
	sub r3, r3, r1						// R3 = R3 - R1
	jump ch0_check, ov					// Jump if R3 < R1
	move r0, 0							// R0 = 0

ch0_check:
	// State of input in R0 changed?
	move r3, ch0_next_edge				// R3 = @ch0_next_edge
	ld r3, r3, 0						// R3 = ch0_next_edge
	add r3, r0, r3						// R3 = R0 + R3
	and r3, r3, 1						// R3 = R3 & 0x0001
	jump ch0_changed, EQ				// Jump if R3==0

	// Not changed
	// Reset debounce_counter to debounce_max_count
	move r3, debounce_max_count			// R3 = debounce_max_count
	move r2, ch0_debounce_counter		// R2 = @ch0_debounce_counter
	st r3, r2, 0						// ch0_debounce_counter = debounce_max_count

	// Clear GPIO pin ULP_CH0_OUT
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_OUT, 1, 1)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH0_OUT*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_CH0_OUT, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_CH0_OUT, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_OUT, 1, 0)

	// Read next channel
	jump ch0_end

ch0_changed:
	// Input state changed
	// Has debounce_counter reached zero?
	move r3, ch0_debounce_counter		// R3 = @ch0_debounce_counter
	ld r2, r3, 0						// R2 = ch0_debounce_counter
	add r2, r2, 0 						// dummy ADD to use "jump if ALU result is zero"
	jump ch0_edge_detected, EQ			// Jump if R2==0

	// Not yet. Decrement debounce_counter
	sub r2, r2, 1						// R2 = R2 - 1
	st r2, r3, 0						// ch0_debounce_counter = R2

	// Read next channel
	jump ch0_end

ch0_edge_detected:
	// Reset debounce_counter to debounce_max_count
	move r3, debounce_max_count			// R3 = debounce_max_count
	move r2, ch0_debounce_counter		// R2 = @ch0_debounce_counter
	st r3, r2, 0						// ch0_debounce_counter = debounce_max_count
	
	// Flip next_edge
	move r3, ch0_next_edge				// R3 = @ch0_next_edge
	ld r2, r3, 0						// R2 = ch0_next_edge
	move R1, R2							// R1 = ch0_next_edge
	add r2, r2, 1						// R2 = R2 + 1
	and r2, r2, 1						// R2 = R2 & 0x0001
	st r2, r3, 0						// ch0_next_edge = R2

	// Check pulse edge
	add r1, r1, 0 						// dummy ADD to use "jump if current edge is zero"
	jump ch0_edge_negative, EQ			// Jump if R1==0

	// Clear GPIO pin ULP_CH0_LED
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_LED, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH0_LED*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_CH0_LED, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_CH0_LED, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_LED, 1, 1)

	// Clear GPIO pin ULP_CH0_OUT
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_OUT, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH0_OUT*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_CH0_OUT, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_CH0_OUT, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_OUT, 1, 1)

	// Read next channel
	jump ch0_end

ch0_edge_negative:
	// Check use_led
	move r0, use_led					// R0 = @use_led
	ld r0, r0, 0						// R0 = use_led
	jumpr ch0_edge_increment, 0, EQ		// Jump if R0==0
	
	// Set GPIO pin ULP_CH0_LED
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_LED, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH0_LED*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + ULP_CH0_LED, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + ULP_CH0_LED, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_LED, 1, 1)

ch0_edge_out:
	// Check use_out
	move r0, use_out					// R0 = @use_out
	ld r0, r0, 0						// R0 = use_out
	jumpr ch0_edge_increment, 0, EQ		// Jump if R0==0
	
	// Set GPIO pin ULP_CH0_OUT
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_OUT, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH0_OUT*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + ULP_CH0_OUT, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + ULP_CH0_OUT, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH0_OUT, 1, 1)

ch0_edge_increment:
	// Increment pulse count
	move r3, ch0_pulse_count			// R3 = @ch0_pulse_count
	ld r2, r3, 0						// R2 = ch0_pulse_count
	add r2, r2, 1						// R2 = R2 + 1
	st r2, r3, 0						// ch0_pulse_count = R2

	// Measure pin analog voltage if discrete type
	move r0, ch0_type					// R0 = @ch0_type
	ld r0, r0, 0						// R0 = ch0_type
	jumpr ch0_end, 2, EQ				// Jump if R0==2

	//adc r1, 0, ch0_adc + 1				// measure gpio pin voltage
	move r2, ch0_adc_value				// R2 = @ch0_adc_value
	st r1, r2, 0						// ch0_adc_value = R2

ch0_end:
	// Read next channel
	jump ch1_read

//=====================================================================================
	.global ch1_read
ch1_read:
	move r0, ch1_type					// R0 = @ch1_type
	ld r0, r0, 0						// R0 = ch1_type
	jumpr ch1_end, 0, EQ				// Jump if R0==0
	jumpr ch1_analog_read, 2, EQ		// Jump if R0==2

ch1_discrete_read:
	move r2, input						// R2 = @input
	ld r0, r2, 0						// R0 = input
	move r3, ch1_gpio					// R3 = ch1_gpio
	rsh r0, r0, r3						// R0 = input >> ch1_gpio
	and r0, r0, 1						// R0 = R0 & 0x0001
	jump ch1_check

ch1_analog_read:
	adc r1, 0, ch1_adc + 1				// R1 = measured gpio pin voltage
	move r2, ch1_adc_value				// R2 = @ch1_adc_value
	st r1, r2, 0						// ch1_adc_value = R1

	// compare with adc_low_thr
	move r0, 1							// R0 = 0
	move r3, adc_low_thr				// R3 = adc_low_thr
	sub r3, r3, r1						// R3 = R3 - R1
	jump ch1_check, ov					// Jump if R3 < R1
	move r0, 0							// R0 = 1

ch1_check:
	// State of input in R0 changed?
	move r3, ch1_next_edge				// R3 = @ch1_next_edge
	ld r3, r3, 0						// R3 = ch1_next_edge
	add r3, r0, r3						// R3 = R0 + R3
	and r3, r3, 1						// R3 = R3 & 0x0001
	jump ch1_changed, eq				// Jump if R3==0

	// Not changed
	// Reset debounce_counter to debounce_max_count
	move r3, debounce_max_count			// R3 = debounce_max_count
	move r2, ch1_debounce_counter		// R2 = @ch1_debounce_counter
	st r3, r2, 0						// ch1_debounce_counter = debounce_max_count

	// Clear GPIO pin ULP_CH1_OUT
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_OUT, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH1_OUT*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_CH1_OUT, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_CH1_OUT, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_OUT, 1, 1)

	// Read next channel
	jump ch1_end

ch1_changed:
	// Input state changed
	// Has debounce_counter reached zero?
	move r3, ch1_debounce_counter		// R3 = @ch1_debounce_counter
	ld r2, r3, 0						// R2 = ch1_debounce_counter
	add r2, r2, 0 						// dummy ADD to use "jump if ALU result is zero"
	jump ch1_edge_detected, eq			// Jump if R2==0

	// Not yet. Decrement debounce_counter
	sub r2, r2, 1						// R2 = R2 - 1
	st r2, r3, 0						// ch1_debounce_counter = R2

	// Read next channel
	jump ch1_end

ch1_edge_detected:
	// Reset debounce_counter to debounce_max_count
	move r3, debounce_max_count			// R3 = debounce_max_count
	move r2, ch1_debounce_counter		// R2 = @ch1_debounce_counter
	st r3, r2, 0						// ch1_debounce_counter = debounce_max_count
	
	// Flip next_edge
	move r3, ch1_next_edge				// R3 = @ch1_next_edge
	ld r2, r3, 0						// R2 = ch1_next_edge
	move R1, R2							// R1 = ch1_next_edge
	add r2, r2, 1						// R2 = R2 + 1
	and r2, r2, 1						// R2 = R2 & 0x0001
	st r2, r3, 0						// ch1_next_edge = R2

	// Check pulse edge
	add r1, r1, 0 						// dummy ADD to use "jump if current edge is zero"
	jump ch1_edge_negative, eq			// Jump if R1==0

	// Clear GPIO pin ULP_CH1_LED
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_LED, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH1_LED*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_CH1_LED, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_CH1_LED, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_LED, 1, 1)

	// Clear GPIO pin ULP_CH1_OUT
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_OUT, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH1_OUT*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_CH1_OUT, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_CH1_OUT, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_OUT, 1, 1)

	// Read next channel
	jump ch1_end

ch1_edge_negative:
	// Check use_led
	move r0, use_led					// R0 = @use_led
	ld r0, r0, 0						// R0 = use_led
	jumpr ch1_edge_out, 0, EQ			// Jump if R0==0
	
	// Set GPIO ULP_CH1_LED
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_LED, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH1_LED*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + ULP_CH1_LED, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + ULP_CH1_LED, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_LED, 1, 1)

ch1_edge_out:
	// Check use_out
	move r0, use_out					// R0 = @use_out
	ld r0, r0, 0						// R0 = use_out
	jumpr ch1_edge_increment, 0, EQ		// Jump if R0==0
	
	// Set GPIO ULP_CH1_OUT
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_OUT, 1, 0)
	WRITE_RTC_FIELD(RTC_IO_TOUCH_PAD0_REG + ULP_CH1_OUT*4, RTC_IO_TOUCH_PAD0_MUX_SEL, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + ULP_CH1_OUT, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + ULP_CH1_OUT, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_CH1_OUT, 1, 1)

ch1_edge_increment:
	// Increment pulse count
	move r3, ch1_pulse_count			// R3 = @ch1_pulse_count
	ld r2, r3, 0						// R2 = ch1_pulse_count
	add r2, r2, 1						// R2 = R2 + 1
	st r2, r3, 0						// ch1_pulse_count = R2

	// Measure pin analog voltage if discrete type
	move r0, ch1_type					// R0 = @ch1_type
	ld r0, r0, 0						// R0 = ch1_type
	jumpr ch1_end, 2, EQ				// Jump if R0==2

	//adc r1, 0, ch1_adc + 1				// measure gpio pin voltage
	move r2, ch1_adc_value				// R2 = @ch1_adc_value
	st r1, r2, 0						// ch1_adc_value = R2

ch1_end:
	// Clear GPIO pin ULP_PULL_UP, hold it
	WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + ULP_PULL_UP, 1, 1)
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + ULP_PULL_UP, 1, 1)
	WRITE_RTC_REG(RTC_CNTL_PAD_HOLD_REG, ULP_PULL_UP, 1, 1)

	// Read next channel
	jump check_wake_up
