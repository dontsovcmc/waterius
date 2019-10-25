/*
 * JoBa1 ESP32 ULP blink example
 *
 * Simple ULP program that initializes a gpio pin and toggles it high/low
 */

#include "soc/soc_ulp.h"     // for WRITE_RTC_REG
#include "soc/rtc_io_reg.h"  // for RTC_GPIO_*


.global entry                // ulp start address used by main core program

.set gpio_2, 12              // gpio pin 2 is rtc pin 12

.text

entry:
  // might be needed for some pads, but surely not #2
  // WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG, RTC_IO_TOUCH_PAD2_TO_GPIO_S, 1, 1)

  // use digital function, not rtc function
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG, RTC_IO_TOUCH_PAD2_MUX_SEL_S, 1, 1)

  // gpio_2 shall be output, not input
  WRITE_RTC_REG(RTC_GPIO_OUT_REG, RTC_GPIO_OUT_DATA_S + gpio_2, 1, 1)

on:
  WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + gpio_2, 1, 1)

  move  r1, 200           // wait in ms
  move  r2, off           // return address
  jump  delay             // call subroutine

off:
  WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + gpio_2, 1, 1)

  move  r1, 800           // wait in ms
  move  r2, on            // return address
  jump  delay             // call subroutine

delay:
  wait  8000              // wait 8000 clock ticks at 8MHz -> 1ms
  sub   r1, r1, 1         // decrement ms count
  jump  r2, eq            // if ms count is zero then return to caller
  jump  delay             // else continue to wait