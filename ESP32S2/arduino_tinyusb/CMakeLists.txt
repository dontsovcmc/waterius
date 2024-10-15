idf_component_register(REQUIRES esp_rom freertos soc PRIV_REQUIRES)

if(CONFIG_TINYUSB_ENABLED)

  ### variables ###
  #################
  # if(IDF_TARGET STREQUAL "esp32s2")
    set(compile_options
        "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
        "-DCFG_TUSB_DEBUG=${CONFIG_TINYUSB_DEBUG_LEVEL}"
        "-Wno-type-limits" # needed for the vanila tinyusb with turned off classes
    )
  # elseif(IDF_TARGET STREQUAL "esp32s3")
  #   set(compile_options
  #       "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
  #       "-DCFG_TUSB_DEBUG=${CONFIG_TINYUSB_DEBUG_LEVEL}"
  #       "-Wno-type-limits" # needed for the vanila tinyusb with turned off classes
  #   )
  # endif()
  idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos
                             ORIG_INCLUDE_PATH)
  set(includes_private
      # tusb:
      "${COMPONENT_DIR}/tinyusb/hw/bsp/"
      "${COMPONENT_DIR}/tinyusb/src/"
      "${COMPONENT_DIR}/tinyusb/src/device"
      )

  set(includes_public
      # tusb:
      "${FREERTOS_ORIG_INCLUDE_PATH}"
      "${COMPONENT_DIR}/tinyusb/src/"
      # espressif:
      "${COMPONENT_DIR}/include")
  set(srcs
      # espressif:
      "${COMPONENT_DIR}/src/dcd_esp32sx.c"
      # tusb:
      #"${COMPONENT_DIR}/tinyusb/src/portable/espressif/esp32sx/dcd_esp32sx.c"
      "${COMPONENT_DIR}/tinyusb/src/class/cdc/cdc_device.c"
      "${COMPONENT_DIR}/tinyusb/src/class/hid/hid_device.c"
      "${COMPONENT_DIR}/tinyusb/src/class/midi/midi_device.c"
      "${COMPONENT_DIR}/tinyusb/src/class/msc/msc_device.c"
      "${COMPONENT_DIR}/tinyusb/src/class/video/video_device.c"
      "${COMPONENT_DIR}/tinyusb/src/class/dfu/dfu_rt_device.c"
      "${COMPONENT_DIR}/tinyusb/src/class/vendor/vendor_device.c"
      "${COMPONENT_DIR}/tinyusb/src/common/tusb_fifo.c"
      "${COMPONENT_DIR}/tinyusb/src/device/usbd_control.c"
      "${COMPONENT_DIR}/tinyusb/src/device/usbd.c"
      "${COMPONENT_DIR}/tinyusb/src/tusb.c")

  ### tinyusb lib ###
  ###################
  add_library(arduino_tinyusb STATIC ${srcs})
  target_include_directories(
    arduino_tinyusb
    PUBLIC ${includes_public}
    PRIVATE ${includes_private})
  target_compile_options(arduino_tinyusb PRIVATE ${compile_options})
  target_link_libraries(${COMPONENT_TARGET} INTERFACE arduino_tinyusb)

endif()
