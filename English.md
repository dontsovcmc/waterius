### Get water meter data sent to your mobile phone using Wi-Fi (powered by AA batteries). 

# Waterius 

Full article on [Hackster.io](https://www.hackster.io/dontsovcmc/waterius-4bfaba)

### Waterius can sends:
- current values everyday
- delta values (per day)
- voltage
- e-mail by Blynk (title and message template)
- data to your TCP server
- low voltage detector (experimental)
- alarms: high flow, continuous flow, stopped consumption, floor water sensor (experimental,
  see [Alarms.md](Alarms.md) — in Russian)

Values saved in ATtiny EEPROM cycle buffer (~2 millions cycles). "Software ESD protection".

### How It Works
*Two chips*: ATtiny85 in deep sleep mode. It counts impulses and wakes up ESP-01 to transfer data to Blynk every day. Chips talks by i2c line.

*Power*: 3*AA batteries (or 2*AA lithium batteries without voltage regulator)

*Current*: 15-24 uA in deep sleep, 75 mA when transmitting for ~5 sec

*Lifetime*: estimated 4 year battery life

*Limits*: 1 impulse per second (increased by constant in code)

*Inputs*: 2 water meters. An input can be used for a wired floor water sensor instead of a
meter: any conductivity between the probes raises the alarm — water is tens and hundreds of
kilohms, not a contact closure. A normally closed sensor is wired as a loop (a jumper or a
5.6k resistor) and alarms on higher resistance, a cut wire included.

This page is an old summary; the up-to-date documentation is in Russian, see
[README.md](README.md).
