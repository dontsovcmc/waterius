# Waterius 2.0.54
## Wi-Fi add-on for pulse-output water, gas and heat meters

<a href="https://github.com/dontsovcmc/waterius/actions/workflows/ci.yml" target="_blank"><img src="https://github.com/dontsovcmc/waterius/actions/workflows/ci.yml/badge.svg?branch=master"></a>

![waterius2-phone_](https://user-images.githubusercontent.com/3930496/149906477-6aa47cdd-f714-4546-85ff-5541c60260a2.jpeg)

[На русском](https://github.com/dontsovcmc/waterius/blob/master/README.md)

This page is a translation of [README.md](README.md). The rest of the documentation is in Russian.

Full article on [Hackster.io](https://www.hackster.io/dontsovcmc/waterius-4bfaba)

✅ HTTP and MQTT protocols

✅ Works with <a href="https://yandex.ru/alice/smart-home" target="_blank">Yandex Smart Home</a>, <a href="https://www.home-assistant.io/" target="_blank">Home Assistant</a>, <a href="https://www.hackster.io/dontsovcmc/domoticz-4346d5" target="_blank">Domoticz</a>, <a href="https://spruthub.ru" target="_blank">SprutHub</a>

✅ Personal account at <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github" target="_blank">waterius.ru</a>

- automatic <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github" target="_blank">submission of meter readings</a> to 157 water utilities in Russia and the CIS (as of September 2025)
- official partner of <a href="https://mos.ru" target="_blank">mos.ru</a>
- readings by e-mail
- readings by SMS
- Telegram bot

✅ Runs for several years on 3 AA batteries

✅ [Alarms](Alarms.md) (experimental, in Russian): a lot of water at once, leak, stopped consumption, floor water sensor

### Suitable meters
✅ Any water meter with a [pulse output](https://github.com/dontsovcmc/waterius/issues/65) (a wire coming out of the body)

✅ Any gas meter with a reed switch

✅ Electronic gas meters: Betar SGBM-4 [details](https://github.com/dontsovcmc/waterius/issues/233)

✅ Electronic heat meters: Sanext Monu CU, Beril ITELMA STE 31 [details](https://github.com/dontsovcmc/waterius/issues/233)

✅ Electricity meters: Energomera CE101, Energomera CE6803V 10-100A M7 R31 and probably other pulse-output meters.
The electronic input has been tested at 20 pulses per second: enough for a 3200 imp/kWh meter up to 22 kW (attiny firmware 47 or newer)


### Where to buy

[waterius.ru](https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github)

### Features
- up to two meters
- 3 AA batteries (~2-4 years)
- Wi-Fi transfer
- no need to know the pulse weight of a water meter (Waterius detects 1 or 10 l/pulse by itself)
- no need to know the water meter output type: "dry contact" or "NAMUR"
- configurable sending period
- a floor water sensor can be wired to an input instead of a meter: the alarm is raised by water
  between the probes, not only by a contact closure ([Alarms](Alarms.md), attiny firmware 44 or newer)
- manual pulse weight setting
- Home Assistant discovery (there is also a HAOS integration)
- your own web server, MQTT broker and NTP server can be set
- SprutHub support
- WirenBoard support

#### Waterius data can be seen:
* on <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github">waterius.ru</a>
* in the <a href="https://yandex.ru/alice/smart-home">Yandex Smart Home with Alice</a> app
* in Home Assistant: [guide](Home_Assistant_setup.md), [configuration](https://github.com/dontsovcmc/waterius/blob/master/homeassistant.configuration.yaml), [discussion](https://github.com/dontsovcmc/waterius/issues/86) or [integration](https://github.com/Anat0l/waterius-ha)
* in Domoticz: [guide](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
* in SprutHub: [guide](https://wiki.spruthub.ru/%D0%A1%D0%BE%D0%B7%D0%B4%D0%B0%D0%BD%D0%B8%D0%B5_%D0%BA%D0%BE%D0%BD%D1%82%D1%80%D0%BE%D0%BB%D0%BB%D0%B5%D1%80%D0%B0_MQTT)
* on your own [HTTP/HTTPS server](https://github.com/dontsovcmc/waterius/blob/master/Export.md#%D0%BD%D0%B0%D1%81%D1%82%D1%80%D0%BE%D0%B9%D0%BA%D0%B0-%D0%BE%D1%82%D0%BF%D1%80%D0%B0%D0%B2%D0%BA%D0%B8-%D0%BF%D0%BE-http-%D1%81%D0%B2%D0%BE%D0%B9-%D1%81%D0%B5%D1%80%D0%B2%D0%B5%D1%80)
* in an MQTT client: [fields and setup](https://github.com/dontsovcmc/waterius/blob/master/Export.md#настройка-отправки-по-mqtt)
* wherever you program it to go
  * [httpwaterius](https://github.com/grffio/httpwaterius) - a web server with a simple UI by [grffio](https://github.com/grffio)
* straight to your housing management company through [waterius.ru](https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github)

## Connection and setup
[Text](http://waterius.ru/manual?utm_source=github)
[Video](https://www.youtube.com/watch?v=dsmIdWbqJ58)

## Alternatives
[Commercial and DIY](https://github.com/dontsovcmc/waterius/issues/10)

## DIY
[Download firmware](https://github.com/dontsovcmc/waterius/releases)

[Flash the ESP from a browser](https://waterius.github.io/update/): the page writes a downloaded `.bin` to the ESP8266 over USB-TTL, with nothing to install (Chrome or Edge)

[Update Waterius 2 over the air](https://account.waterius.ru/update/) from your personal account: upload the firmware and the file system, and the update starts when the device next connects

Alpha version (built from the `dev` branch, for those ready to try new things): [2.0.54-47-alpha](https://github.com/dontsovcmc/waterius/releases/tag/2.0.54-47-alpha)

Community guide in the Telegram chat info: [waterius_forum](https://t.me/waterius_forum)
- [Parts list and making the board](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [Flashing Attiny85 and ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md)
- [Installation and setup](https://github.com/dontsovcmc/waterius/blob/master/Setup.md)

### Reed switch for a gas meter
Reed switch case for the BK-G4T gas meter: [bk-g4t-sensor.zip](https://github.com/dontsovcmc/waterius/files/10365883/bk-g4t-sensor.zip)

## How it works
The pulse counter has two chips. The Attiny85 counts pulses while sleeping and stores them in EEPROM. Once a day it wakes up the ESP8266 and listens on the i2c line. The ESP8266 asks the Attiny85 for the data and sends it to the server. Then the ESP8266 goes to sleep, and the Attiny85 keeps counting, counting, counting...

### Known issues
- Sometimes connects unreliably to some Asus and Keenetic routers. Set "only G" in the router Wi-Fi settings for Waterius.
- Before 0.11.8: sometimes does not connect to Rostelecom routers: Sercomm rv6699, Innbox e70, e80 (192.168.0.1), TP-Link AX5400. A static IP in the settings fixes it. If you have one of these, write in the topic: [Router problems](https://github.com/dontsovcmc/waterius/issues/131)
- Before 0.11.7: does not connect to a Wi-Fi network with a space in its name.

## Schematic
Factory board:
![waterius board](https://github.com/dontsovcmc/waterius/blob/master/Board/board_3_3.jpg)

The repository also has a single-layer board for toner transfer.

## Development
The dev branch is for pull requests.
The master branch is only for publishing firmware.

**Portal simulator** — the setup pages open in a browser just as on the device, while pulses,
the button and Wi-Fi are simulated from a control panel. Handy for looking at the portal without
building the firmware: [simulator/README.md](simulator/README.md).

**How the firmware works** — [docs/README.md](docs/README.md): pulse counting, scheduling, sending, alarms, portal, OTA.

**Tests:**
- ESP business logic on a computer, no hardware needed: `ESP8266/scripts/run_tests.sh`, attiny — `Attiny85/scripts/run_tests.sh`;
- hardware-in-the-loop bench: a real Waterius, a controlled access point and its own broker — [Utils/hil/README.md](Utils/hil/README.md).

Hard tasks still open:
- ~~Write the log to a file (LittleFS) so it can be downloaded (/logs.html)~~ — done: the portal journal is kept in memory and downloaded from the "Логи" page ([description](docs/setup-portal.md#журнал-и-список-сетей))
- ~~OTA updates: propose firmware code and a sample web server~~ — done in [2.0.43-beta](https://github.com/dontsovcmc/waterius/releases/tag/v.2.0.43-beta): Waterius 2 updates over the air on a command from the server or MQTT, from the personal account on the [update page](https://account.waterius.ru/update/) ([description](ESP8266/doc/ota.md))

### Modifications
The [attiny84 branch](https://github.com/dontsovcmc/waterius/tree/attiny84) supports the [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F) board with 4 meters and 2 leak sensors.

[Waterius on ESP32 with NB-IoT](https://github.com/OloloevReal/Waterius32) by OloloevReal

Leak sensor:
- On the reset pin, made by [OloloevReal](https://github.com/OloloevReal), here is the [schematic](https://github.com/dontsovcmc/waterius/issues/51)
- Available in the [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F) clone, thanks to [badenbaden]

# Disclaimer
The Waterius firmware is built on open-source libraries, and nobody guarantees that they work. I also cannot promise that the device will work with your equipment or that you will not suffer damage while making or using it =). Please report any experience of making and using it [here](https://github.com/dontsovcmc/waterius/issues). You will help the project grow!
[GNU LGPLv3 license](https://github.com/dontsovcmc/waterius/blob/master/LICENSE)

# Thanks
- [marvel-m9y](https://github.com/marvel-m9y) for HTTPS support
- [OloloevReal](https://github.com/OloloevReal) for the leak sensor and the ESP32 work
- [popsodav](https://github.com/popsodav) for MQTT
- Ivan Kovalenko and Ivan Ganzha for advice on electrical engineering
- Alex Jensen, for the [temperature sensor](https://www.cron.dk/esp8266-on-batteries-for-years-part-1) project
- [freenetwork](https://github.com/freenetwork) for the Home Assistant configuration
- [grffio](https://github.com/grffio) for the local web server
- [Igor Vakhromeev](http://vakhromeev.com) for the awesome settings redesign
- Sergey A. (Murmansk) for the detailed guide on [setting up Domoticz and NodeRed](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
- [sintech](https://github.com/sintech) for bugs found and fixed
- [zinger76](https://github.com/zinger76) for the board ordering link and the [3D model of the wall mount](https://github.com/dontsovcmc/waterius/blob/master/wall-mount/wall_mount.md)
- [badenbaden](https://github.com/badenbaden) for useful comments on production and the new version!
- [kick2nick](https://github.com/kick2nick) for feature improvements
- [foxel](https://github.com/foxel) for board improvements
- Users who sent typos and suggestions: Dmitry (Moscow), Sergey (Kostroma), Alexander (Saint Petersburg), Sergey (Murmansk), Anton (Krasnoyarsk) and others
- Denis S. (Moscow) for the Waterius installation video
- [ivakorin](https://github.com/ivakorin) for the Home Assistant guide
- Evgeny K. from Samara and Oleg from Moscow for criticism and help with the firmware
- Oleg K. from Belarus for the [MajorDoMo guide](https://mjdm.ru/forum/viewtopic.php?p=129000#p129000)
- [Drafteed](https://github.com/Drafteed) for the map of Russia widget
- [neitri](https://github.com/neitri) for firmware improvements
- [L2jLiga](https://github.com/L2jLiga) for updating the Home Assistant configuration
- nyroux for the reed switch case for the gas meter
- Anat0liyBM for HA discovery support
- [vzagorovskiy](https://github.com/vzagorovskiy) for the big [0.11.0](https://github.com/dontsovcmc/waterius/releases/tag/0.11.0-beta) update
- [abrant-ru](https://github.com/abrant-ru) for supporting meters with an "open collector" output (short pulses)
- Daniil Makarov for the web interface design
- [videlinagbm](https://github.com/videlinagbm) for the web interface markup and JS code
- [Anat0l](https://github.com/Anat0l/waterius-ha) for writing the HA integration

Forums:
- https://electronix.ru
- https://esp8266.ru
- https://easyelectronics.ru

## Star History

<img src="https://star-history.dera.page/svg?repos=dontsovcmc/waterius&type=Date" width="400">

## Contacts

Contact: [Facebook](https://www.facebook.com/waterius), [VK](https://vk.com/waterius1), [Instagram](https://www.instagram.com/waterius.ru/)

Ask questions in the Telegram chat: [waterius_forum](https://t.me/waterius_forum)

Report bugs [in issues](https://github.com/dontsovcmc/waterius/issues)
