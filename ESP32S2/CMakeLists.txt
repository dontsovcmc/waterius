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
list(APPEND compile_definitions "CONFIG_AUTOSTART_ARDUINO=1")
list(APPEND compile_definitions "CONFIG_TINYUSB_ENABLED=0")
list(APPEND compile_definitions "CONFIG_TINYUSB_CDC_ENABLED=0")
list(APPEND compile_definitions "CONFIG_USB_ENABLED=0")
list(APPEND compile_definitions "CONFIG_USB_CDC_ENABLED=0")

#list(APPEND compile_definitions "USE_TINYUSB")
#list(APPEND compile_definitions "CFG_TUSB_MCU=OPT_MCU_ESP32S2")

list(APPEND compile_definitions "ARDUINO_USB_CDC_ON_BOOT=0")
list(APPEND compile_definitions "ARDUINO_USB_MODE=0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

#list(APPEND EXTRA_COMPONENT_DIRS tinyusb)
#list(APPEND EXTRA_COMPONENT_DIRS esp_tinyusb)

list(APPEND EXTRA_COMPONENT_DIRS arduino_tinyusb)
include_directories(arduino_tinyusb/tinyusb/src/class/cdc)

#list(APPEND EXTRA_COMPONENT_DIRS esp32-arduino-lib-builder)

#include_directories(managed_components/espressif__tinyusb/src)
#include_directories($ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos)
#include_directories(managed_components/espressif__tinyusb/src)
#include_directories(esp_tinyusb/include)

project(waterius-esp32)
