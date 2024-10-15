/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files
 */
#include "soc/rtc_cntl_reg.h"
#include "soc/soc_ulp.h"
#include "sdkconfig.h"
#include "ulp_config.h"

	.set button_gpio, ULP_BUTTON_IO
	.set batt_en_gpio, ULP_BATT_EN_IO

	.global wake_up_period
wake_up_period:				// Number of starts before waking up the SoC
	.long 0

	.global wake_up_counter
wake_up_counter:			// Current number of starts 
	.long 0

	.global wake_up_event
wake_up_event:				// Wake up event: 1=time, 2=key, 3=usb
	.long 0

	.global check_wake_up
check_wake_up:
	// Check if the system is in sleep mode
#if CONFIG_IDF_TARGET_ESP32
	READ_RTC_REG(RTC_CNTL_LOW_POWER_ST_REG, 27, 1)
#else
	READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_MAIN_STATE_IN_IDLE)
#endif
	jumpr no_sleep, 1, EQ				// Jump if R0==1

	// Check button
	move r2, input						// R2 = @input
	ld r0, r2, 0						// R0 = input
	move r3, button_gpio				// R3 = button_gpio
	rsh r0, r0, r3						// R0 = input >> button_gpio
	and r0, r0, 1						// R0 = R0 & 0x0001
	jumpr wake_up_key, 0, EQ			// Jump if R0==0

	// Check usb power
	move r2, input						// R2 = @input
	ld r0, r2, 0						// R0 = input
	move r3, batt_en_gpio				// R3 = batt_en_gpio
	rsh r0, r0, r3						// R0 = input >> batt_en_gpio
	and r0, r0, 1						// R0 = R0 & 0x0001
	jumpr wake_up_usb, 0, EQ			// Jump if R0==0

no_sleep:
	// Compare wake_up_counter to wake_up_period
	move r1, wake_up_counter			// R1 = @wake_up_counter
	ld r2, r1, 0						// R2 = wake_up_counter
	move r3, wake_up_period				// R3 = @wake_up_period
	ld r3, r3, 0						// R3 = wake_up_period
	sub r3, r3, r2						// R3 = R3 - R2
	jump wake_up_time, eq				// Jump if R3==R2
	jump wake_up_time, ov				// Jump if R3 < R2 (wake_up_period < wake_up_counter)

	add r2, r2, 1						// R2 = R2 + 1
	st r2, r1, 0						// wake_up_counter = R2

	// End program
	halt

wake_up_time:
	move r0, 1
	jump wake_up

wake_up_key:
	move r0, 2
	jump wake_up

wake_up_usb:
	move r0, 3
	jump wake_up

	// End the program
	.global exit
exit:
	halt

	// Stop ULP and end the program
	.global stop_and_exit
stop_and_exit:
	WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
	halt

	.global wake_up
wake_up:
	// Store event from R0
	move r2, wake_up_event				// R2 = @wake_up_event
	st r0, r2, 0						// wake_up_event = R0

	// Clear wake_up_counter
	move r1, wake_up_counter			// R1 = @wake_up_counter
	move r2, 0							// R2 = 0
	st r2, r1, 0						// wake_up_counter = R2

	// Check if the system is in sleep mode
#if CONFIG_IDF_TARGET_ESP32
	READ_RTC_REG(RTC_CNTL_LOW_POWER_ST_REG, 27, 1)
#else
	READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_MAIN_STATE_IN_IDLE)
#endif
	move r1, r0							// R1 = R0

	// Check if the system can be woken up
	READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
	
	// If the system is in normal mode or if the system is in sleep mode with ready for wakeup set, we can signal the main CPU to wakeup
	or r0, r0, r1						// R0 = R0 || R1
	jump wake_up, eq					// Jump if R0==0

	// Wake up the SoC, end program
	wake
	halt
