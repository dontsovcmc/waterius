cmake_minimum_required(VERSION 3.16.0)

if(EXISTS "${SDKCONFIG}.defaults")
    if(EXISTS "sdkconfig.defaults")
        set(SDKCONFIG_DEFAULTS "${SDKCONFIG}.defaults;sdkconfig.defaults")
    else()
        set(SDKCONFIG_DEFAULTS "${SDKCONFIG}.defaults")
    endif()
    message(STATUS "-- Using defaults: ${SDKCONFIG_DEFAULTS}")
else()
    message(STATUS "-- No defaults SDKCONFIG")
endif()

# Adds necessary definitions for compiling it using Serial symbol attached to the HW USB CDC port
#list(APPEND compile_definitions "CONFIG_AUTOSTART_ARDUINO=1")

#list(APPEND compile_definitions "CONFIG_TINYUSB_ENABLED=1")
#list(APPEND compile_definitions "CONFIG_TINYUSB_CDC_ENABLED=1")
#list(APPEND compile_definitions "CONFIG_USB_ENABLED=1")
#list(APPEND compile_definitions "CONFIG_USB_CDC_ENABLED=1")
#list(APPEND compile_definitions "CONFIG_TINYUSB_CDC_RX_BUFSIZE=512")


#list(APPEND compile_definitions "CFG_TUSB_MCU=OPT_MCU_ESP32S2")
#list(APPEND compile_definitions "CFG_TUSB_OS_INC_PATH=freertos/")

#list(APPEND compile_definitions "ARDUINO_USB_CDC_ON_BOOT=1")
#list(APPEND compile_definitions "ARDUINO_USB_MODE=0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

include_directories(src/usb)
include_directories(.pio/libdeps/esp32s2/TinyUSB/src)

project(waterius-esp32)
