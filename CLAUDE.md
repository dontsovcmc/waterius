# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Waterius is a WiFi adapter for impulse counters (water, gas, heat, electricity). It uses a dual-microcontroller design: an ATtiny85 for ultra-low-power impulse counting and an ESP8266 for WiFi connectivity and cloud integration. The project documentation and comments are primarily in Russian.

## Build Commands

The build system is PlatformIO. The binary is at `~/.platformio/penv/bin/pio`.

**ESP8266 firmware:**
```bash
~/.platformio/penv/bin/pio run -d ESP8266                    # build (default env: esp01_1m)
~/.platformio/penv/bin/pio run -d ESP8266 -e nodemcuv2       # build for NodeMCU
~/.platformio/penv/bin/pio run -d ESP8266 -t upload           # flash firmware
~/.platformio/penv/bin/pio run -d ESP8266 -t uploadfs         # flash LittleFS filesystem
```

**Attiny85 firmware:**
```bash
~/.platformio/penv/bin/pio run -d Attiny85                    # build
~/.platformio/penv/bin/pio run -d Attiny85 -t upload          # flash via USBasp
```

**CI validation (what Travis runs):**
```bash
~/.platformio/penv/bin/pio ci --project-conf=./ESP8266/platformio.ini ./ESP8266
~/.platformio/penv/bin/pio ci --project-conf=./Attiny85/platformio.ini ./Attiny85
```

**Local secrets:** Copy `ESP8266/secrets.ini.template` to `ESP8266/secrets.ini` and fill in credentials.

## Architecture

### Dual-MCU Data Flow
```
Attiny85 (1MHz, always-on, sleeping)
  → Counts impulses from 2 counter inputs
  → Stores counts in EEPROM with CRC
  → Periodically wakes ESP8266 via I2C/GPIO
      ↓
ESP8266 (80MHz, wakes on demand)
  → Reads AttinyData from Attiny85 over I2C (master_i2c.h)
  → Loads Settings from EEPROM (config.h, setup.h)
  → Calculates display values (CalculatedData)
  → Mode check:
      SETUP_MODE → Starts AP at 192.168.4.1, runs web portal
      TRANSMIT_MODE → WiFi connect → NTP sync → Send data → Sleep
```

### Key Data Structures (ESP8266/src/setup.h)
- **Settings** (~960 bytes, EEPROM) — WiFi credentials, server endpoints, counter config, period settings. Size is compile-time asserted: `static_assert((sizeof(Settings) == 960))`. Changing this struct requires updating the assertion.
- **AttinyData** — Impulse counts, ADC levels, counter types, version, CRC
- **CalculatedData** — Computed meter readings, deltas

### ESP8266 Source Layout (`ESP8266/src/`)
- `main.cpp` — Entry point: setup() → loop() with wake/read/send/sleep cycle
- `portal/` — Web configuration portal (ESPAsyncWebServer)
  - `active_point.cpp` — AP and web server setup
  - `active_point_api.cpp` — REST API handlers, settings validation and save logic (`applyInputParameter`, `applyCheckBoxParameter`, `applyNonCheckBoxParameter`)
- `ha/` — Home Assistant MQTT integration
  - `discovery_entity.cpp` — Builds HA discovery JSON payloads
  - `publish_discovery.cpp` — Publishes entity configs to MQTT
  - `subscribe.cpp` — MQTT subscription and callback handling
  - `apply_settings.cpp` — Applies settings received remotely
- `senders/` — Data transmission
  - `sender_waterius.h` — HTTP/HTTPS to waterius.ru
  - `sender_http.h` — Generic HTTP POST
  - `sender_mqtt.h` — MQTT publish with HA auto-discovery. Contains global `mqtt_client` and `wifi_client` instances.
- `json.cpp` — Serializes all telemetry into a JsonDocument via `get_json_data()`

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

## Branch Strategy

- `dev` — Active development, submit PRs here
- `master` — Firmware releases only
