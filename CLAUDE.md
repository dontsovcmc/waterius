# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Waterius is a Wi-Fi-enabled pulse counter for water, gas, heat, and electricity meters. It consists of two microcontrollers working together:

- **Attiny85**: Sleeps most of the time, counting pulses from meters and storing them in EEPROM. Wakes up ESP periodically or on button press.
- **ESP8266**: Wakes up, retrieves data from Attiny85 via I2C, sends data to servers (HTTP/MQTT), then sleeps.

The device runs on 3 AA batteries for 2-4 years.

## Build Commands

This project uses PlatformIO. Before building ESP8266 firmware, copy `ESP8266/secrets.ini.template` to `ESP8266/secrets.ini` and fill in your credentials.

### Attiny85 Firmware
```bash
cd Attiny85
platformio run                          # Build
platformio run --target upload          # Upload via USBasp programmer
```

### ESP8266 Firmware
```bash
cd ESP8266
platformio run --environment esp01_1m          # Build for ESP-01 1MB
platformio run --environment waterius_2        # Build for Waterius-2 (NodeMCU)
platformio run --target upload --environment esp01_1m    # Upload firmware
platformio run --target uploadfs --environment esp01_1m  # Upload filesystem (LittleFS)
```

### Flash pre-compiled binaries (ESP8266)
```bash
python -m esptool --port /dev/cu.usbserial-XXX --baud 115200 write_flash \
    --flash_freq 40m --flash_size 1MB --flash_mode qio \
    0x0 esp01_1m-X.X.X.bin 0xbb000 esp01_1m-X.X.X-fs.bin
```

### Attiny85 fuses
```
E:FF, H:DF, L:62
```

## Architecture

### Communication Flow
1. Attiny85 counts pulses during sleep (250ms watchdog intervals)
2. Attiny85 wakes ESP via EN pin after configured period (default 15 min)
3. ESP queries Attiny85 via I2C for mode and counter data
4. ESP connects to WiFi, sends data to configured endpoints
5. ESP tells Attiny85 to sleep, then enters deep sleep itself
6. Attiny85 cuts ESP power after 20ms delay

### Key Data Structures
- `Header` struct in `Attiny85/src/Setup.h`: I2C data exchange format (24 bytes)
- `Settings` struct in `ESP8266/src/config.h`: EEPROM configuration (960 bytes)

### Hardware Models
- `WATERIUS_MODEL=0` (MODEL_CLASSIC): Original Waterius with ESP-01
- `WATERIUS_MODEL=2` (MODEL_2): Waterius-2 with ESP-12F and LED indicators

### ESP8266 Source Structure
- `main.cpp`: Entry point, orchestrates wake/send/sleep cycle
- `master_i2c.cpp`: I2C master communication with Attiny85
- `portal/active_point.cpp`: Captive portal for WiFi setup
- `senders/`: HTTP/MQTT/Waterius.ru data senders
- `ha/`: Home Assistant MQTT discovery
- `data/`: Web interface HTML files (served via LittleFS)

### Attiny85 Source Structure
- `main.cpp`: Counter logic, sleep management, I2C slave
- `counter.h`: Pulse detection algorithms (supports dry contact, NAMUR, Hall sensor)
- `Storage.cpp`: Ring buffer EEPROM storage for counter values
- `SlaveI2C.cpp`: I2C slave implementation

## Development Notes

- Logging in Attiny85: Uncomment `LOG_ON` in `Setup.h` (uses PB3/TX pin, disables counter1)
- Logging in ESP8266: Set `LOG_LEVEL_INFO` or `LOG_LEVEL_DEBUG` in build_flags
- ESP32S2 support is experimental (separate `ESP32S2/` directory)
- Pull requests go to `dev` branch; `master` is for releases only
