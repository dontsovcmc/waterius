# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Waterius is a Wi-Fi-enabled pulse counter for water, gas, heat, and electricity meters. It consists of two microcontrollers working together:

- **Attiny85**: Sleeps most of the time, counting pulses from meters and storing them in EEPROM. Wakes up ESP periodically or on button press.
- **ESP8266**: Wakes up, retrieves data from Attiny85 via I2C, sends data to servers (HTTP/MQTT), then sleeps.

The device runs on 3 AA batteries for 2-4 years. The project documentation and comments are primarily in Russian.

## Build Commands

The build system is PlatformIO. The binary is at `~/.platformio/penv/bin/pio`.

**ESP8266 firmware** (default env: `waterius_2`):
```bash
~/.platformio/penv/bin/pio run -d ESP8266                    # build default (waterius_2 / NodeMCU)
~/.platformio/penv/bin/pio run -d ESP8266 -e esp01_1m        # build for Classic Waterius (ESP-01)
~/.platformio/penv/bin/pio run -d ESP8266 -t upload          # flash firmware
~/.platformio/penv/bin/pio run -d ESP8266 -t uploadfs        # flash LittleFS filesystem
```

A third env `nodemcuv2` exists for bench debugging on a bare NodeMCU (verbose WiFi/core logs).

**Attiny85 firmware:**
```bash
~/.platformio/penv/bin/pio run -d Attiny85                    # build
~/.platformio/penv/bin/pio run -d Attiny85 -t upload          # flash via USBasp
```

**Tests** — host-side unit tests (googletest) for the ESP business logic:
```bash
~/.platformio/penv/bin/pio test -d ESP8266 -e native_classic -e native_2   # both models
~/.platformio/penv/bin/pio test -d ESP8266 -e native_2 -f test_ota         # one env, one suite
```

There are two host envs because the two firmwares differ at compile time
(`#if WATERIUS_MODEL`): `native_classic` (`WATERIUS_MODEL=0`) and `native_2`
(`WATERIUS_MODEL=2`). Both must be listed explicitly — a bare `pio test` uses
`default_envs` (`esp01_1m`), where tests are disabled via `test_ignore`. The
`test_model` suite fails the build if `-DWATERIUS_MODEL` is missing, so a
forgotten flag cannot make the two runs silently identical.

The host envs compile **only `src/core/`** (`build_src_filter = -<*> +<core/*>`)
— pure C++ without `Arduino.h`, `String` or hardware access. Anything that needs
Arduino stays in `src/` as an adapter and is not unit-tested. If a test needs
`Arduino.h`, the logic must be moved into `src/core/` first.

`pio test` exits 0 even when it collects no tests (e.g. a `test_filter` that
matches nothing), so CI additionally fails on `0 test cases` in the output.
Plan and suite list: `ESP8266/plan/tests_plan.md`.

**OTA build/deploy:** `ESP8266/scripts/build_and_deploy.sh [version]` builds the `waterius_2` env and stages firmware/filesystem images in `ESP8266/ota/` for upload to the OTA server (URL hardcoded in the script — debug vs. release).

**Local secrets:** Copy `ESP8266/secrets.ini.template` to `ESP8266/secrets.ini` and fill in credentials.

### Manual Flashing

When flashing compiled binaries directly (without PlatformIO upload):

**Find the serial port** (the name changes between sessions/adapters — always detect, never hardcode):
```bash
ls /dev/cu.usbserial-*        # macOS; ignore Bluetooth/audio cu.* devices. If several, ask which.
```

**Attiny85** (via USBasp programmer):
```bash
avrdude -p t85 -c usbasp -B 4 -P usb -U flash:w:"Attiny85/waterius_2-<version>.hex":i
```

**ESP8266** — firmware + LittleFS in a SINGLE esptool session (see gotcha below):
```bash
# Waterius-2 (ESP-12F, 4MB flash) — filesystem at 0x300000
~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 write_flash --flash_freq 40m --flash_size 4MB --flash_mode qio 0x0 ESP8266/.pio/build/waterius_2/firmware.bin 0x300000 ESP8266/.pio/build/waterius_2/littlefs.bin

# Classic (esp01_1m, ESP-01, 1MB flash) — filesystem at 0xBB000
~/.platformio/penv/bin/python -m esptool --port <PORT> --baud 460800 write_flash --flash_freq 40m --flash_size 1MB --flash_mode qio 0x0 ESP8266/.pio/build/esp01_1m/firmware.bin 0xBB000 ESP8266/.pio/build/esp01_1m/littlefs.bin
```
Success = `Hash of data verified.` for BOTH images. Build the images first with `pio run -d ESP8266 -e <env>` and `... -t buildfs`; post_compile.py also copies them to `ESP8266/<env>-<version>.bin` / `-fs.bin`.

**GOTCHA — do NOT flash firmware and filesystem as separate commands** (`pio -t upload` then `pio -t uploadfs`). After the firmware write the ESP hard-resets and boots the running firmware (which enters its I2C/deep-sleep cycle), so auto-reset no longer drops it back into the bootloader — the second upload dies with `Failed to connect to ESP8266: Timed out waiting for packet header`. Flashing both images in one esptool `write_flash` keeps the chip in the bootloader between writes and avoids this.

**Filesystem offset** depends on the LittleFS partition (`board_build.ldscript` in platformio.ini). To derive it for any env: `_FS_start` from the ldscript minus `0x40200000`. esp01_1m uses `eagle.flash.1m256.ld` → `_FS_start=0x402BB000` → `0xBB000`.
```bash
grep _FS_start ~/.platformio/packages/framework-arduinoespressif8266/tools/sdk/ld/<ldscript>.ld
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
- `core/` — pure business logic, no Arduino: compiled into both the firmware and
  the host tests. `core/types.h` holds the data model (`Settings`, `AttinyData`,
  `CalculatedData`, the counter/data enums and the constants describing them)
- `main.cpp` — Entry point: setup() → loop() with wake/read/send/sleep cycle
- `master_i2c.cpp`: I2C master communication with Attiny85
- `portal/` — Web configuration portal (ESPAsyncWebServer)
  - `active_point.cpp` — AP and web server setup
  - `active_point_api.cpp` — REST API handlers, settings validation and save logic
- `ha/` — Home Assistant MQTT integration
  - `subscribe.cpp` — MQTT subscription and callback handling
  - `apply_settings.cpp` — Applies settings received remotely
  - `publish_data.cpp` / `publish_discovery.cpp` — Publishes telemetry and HA auto-discovery payloads
- `senders/` — outbound transports: `sender_http.h`, `sender_mqtt.h`, `sender_waterius.h`, dispatched from `send_data.cpp`
- `json.cpp` — Serializes all telemetry into a JsonDocument via `get_json_data()` (single source of truth for HTTP/MQTT payloads)
- `ota_update.cpp` / `ota_parse.h` — OTA update flow; URL parsing is unit-tested in `test/test_ota`
- `data/` — Web interface HTML files (served via LittleFS, flashed via `uploadfs`)

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
- CI (`.github/workflows/ci.yml`) runs on every PR and push to `dev`/`master`:
  builds both ESP8266 envs and both Attiny85 envs, runs the host tests under
  both models
