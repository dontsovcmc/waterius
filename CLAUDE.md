# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Waterius is a Wi-Fi-enabled pulse counter for water, gas, heat, and electricity meters. It consists of two microcontrollers working together:

- **Attiny85**: Sleeps most of the time, counting pulses from meters and storing them in EEPROM. Wakes up ESP periodically or on button press.
- **ESP8266**: Wakes up, retrieves data from Attiny85 via I2C, sends data to servers (HTTP/MQTT), then sleeps.

The device runs on 3 AA batteries for 2-4 years. The project documentation and comments are primarily in Russian.

## Build Commands

The build system is PlatformIO. The binary is at `~/.platformio/penv/bin/pio`.

**ESP8266 firmware:**
```bash
~/.platformio/penv/bin/pio run -d ESP8266                    # build (default env: esp01_1m)
~/.platformio/penv/bin/pio run -d ESP8266 -e waterius_2      # build for Waterius-2 (NodeMCU)
~/.platformio/penv/bin/pio run -d ESP8266 -t upload           # flash firmware
~/.platformio/penv/bin/pio run -d ESP8266 -t uploadfs         # flash LittleFS filesystem
```

**Attiny85 firmware:**
```bash
~/.platformio/penv/bin/pio run -d Attiny85                    # build
~/.platformio/penv/bin/pio run -d Attiny85 -t upload          # flash via USBasp
```

**Local secrets:** Copy `ESP8266/secrets.ini.template` to `ESP8266/secrets.ini` and fill in credentials.

### Manual Flashing (Waterius-2)

When flashing compiled binaries directly (without PlatformIO upload):

**Attiny85** (via USBasp programmer):
```bash
avrdude -p t85 -c usbasp -B 4 -P usb -U flash:w:"Attiny85/waterius_2-<version>.hex":i
```

**ESP8266** (via USB-serial adapter, e.g. `/dev/cu.usbserial-*` on macOS):
```bash
~/.platformio/penv/bin/python -m esptool --port /dev/cu.usbserial-XXXX --baud 460800 write_flash --flash_freq 40m --flash_size 4MB --flash_mode qio 0x0 ESP8266/waterius_2-<version>.bin 0x300000 ESP8266/waterius_2-<version>-fs.bin
```

Flash order: ATtiny85 first, then ESP8266.

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
- `Settings` struct in `ESP8266/src/setup.h`: EEPROM configuration (960 bytes, compile-time asserted)
- **AttinyData** — Impulse counts, ADC levels, counter types, version, CRC
- **CalculatedData** — Computed meter readings, deltas

### Hardware Models
- `WATERIUS_MODEL=0` (MODEL_CLASSIC): Original Waterius with ESP-01
- `WATERIUS_MODEL=2` (MODEL_2): Waterius-2 with ESP-12F and LED indicators

### ESP8266 Source Structure
- `main.cpp` — Entry point: setup() → loop() with wake/read/send/sleep cycle
- `master_i2c.cpp`: I2C master communication with Attiny85
- `portal/` — Web configuration portal (ESPAsyncWebServer)
  - `active_point.cpp` — AP and web server setup
  - `active_point_api.cpp` — REST API handlers, settings validation and save logic
- `ha/` — Home Assistant MQTT integration
  - `subscribe.cpp` — MQTT subscription and callback handling
  - `apply_settings.cpp` — Applies settings received remotely
- `senders/`: HTTP/MQTT/Waterius.ru data senders
- `json.cpp` — Serializes all telemetry into a JsonDocument via `get_json_data()`
- `data/`: Web interface HTML files (served via LittleFS)

### Attiny85 Source Structure
- `main.cpp`: Counter logic, sleep management, I2C slave
- `counter.h`: Pulse detection algorithms (supports dry contact, NAMUR, Hall sensor)
- `Storage.cpp`: Ring buffer EEPROM storage for counter values
- `SlaveI2C.cpp`: I2C slave implementation

### Compile-Time Feature Flags
Disable modules via build_flags in `platformio.ini`:
- `-DMQTT_DISABLED` — Exclude MQTT
- `-DHTTPS_DISABLED` — Exclude HTTPS sender
- `-DWATERIUS_RU_DISABLED` — Exclude waterius.ru sender

### Library Versions (ESP8266)
- ArduinoJson 7.3.1 (v7 API: `JsonDocument` without size template, `.to<JsonObject>()`)
- PubSubClient 2.8.0
- ESPAsyncWebServer 3.6.0 (ESP32Async fork)
- ESPAsyncTCP 2.0.0

## Development Notes

- Logging in Attiny85: Uncomment `LOG_ON` in `Setup.h` (uses PB3/TX pin, disables counter1)
- Logging in ESP8266: Set `LOG_LEVEL_INFO` or `LOG_LEVEL_DEBUG` in build_flags
- Pull requests go to `dev` branch; `master` is for releases only
