[![Build Status](https://travis-ci.org/puuu/USIWire.svg?branch=master)](https://travis-ci.org/puuu/USIWire)

# USIWire

[Arduino Wire](https://www.arduino.cc/en/Reference/Wire) compatible
library, supporting I²C, I2C, IIC and/or (Two Wire Interface) TWI for
Atmel ATTiny microprocessors utilizing the Universal Serial Interface
(USI).  This library proviedes Master and Slave functionality.


## Installation

This library can easily be installed via
[Arduino Library Manager](https://www.arduino.cc/en/guide/libraries).


## Usage and Support

Please have a look to the
original
[Arduino Wire Library](https://www.arduino.cc/en/Reference/Wire). Just
include the library in your sketch:

```c++
#include <USIWire.h>
```

This library is tested to compile against:
- ATTiny24/ATTiny44/ATTiny84
- ATTiny25/ATTiny45/ATTiny85
- ATTiny261/ATTiny461/ATTiny861
- ATTiny87/ATTiny167
- ATTiny2313/ATTiny4313
- ATTiny1634

ATTiny support for Arduino is provided
by [ATTiny Core](https://github.com/SpenceKonde/ATTinyCore).


## Features

- Provide I2C/TWI for ATTiny platforms with USI support.
- Support for slave and master mode.
- Full callback support in slave mode including proper stop condition
  detection in the interrupt routine.
- Low usage of dynamic memory. Only 46 bytes including buffer.
- Use of one common 32 byte buffer for master and slave.
- Compatibility
  to [Arduino Wire Library](https://www.arduino.cc/en/Reference/Wire)
  1.0 and later.
- Start from scratch with ATMEL "AVR312 USI as I2C slave" and "AVR310
  USI as I2C master" examples.  All changes were done via version
  control.
- Support
  the
  [Arduino Library Manager](https://www.arduino.cc/en/guide/libraries)
  for installation.
- Test suites for master and slave mode that run with USIWire and
  Arduino Wire to validate, e.g., System Management Bus (SMBus)
  communication, proper detection of start and stop condition
  detection, wake up from sleep modes.




## Differences to Arduino Wire Library

To reduce the memory footprint of this library, a common buffer of 32
byte for slave and master mode is used.  This require some care in the
sketches:

- Remove of inheritance of Stream (saves 20 bytes of dynamic
  memory).  Thought, all important `write()` functions are supported.
- One common buffer for slave and master mode: This do not allow, to
  operate simultaneously in master and slave mode.  Thought, switching
  between master and slave mode is possible with `end()` and
  `begin()`.
- Only one buffer for rx and tx operations in master mode: `read()`
  can not be used in between of `beginTransmission()` and
  `endTransmission()`.
- In slave mode, the common buffer is split into rx and tx buffer.
- After calling the onReceive callback, the slave rx buffer is
  cleared.


## Contributing

Contributions are welcome! If you find an bug, feel free to open an
issue at GitHub or even a pull requests ;-).


## Acknowledgement

- Arduino team for
  the [Arduino Wire Library](https://www.arduino.cc/en/Reference/Wire)
- Authors and contributor of TinyWireS and TinyWireM for some
  inspirations.


## Links

- [TinyWireS](https://github.com/rambo/TinyWire/tree/master/TinyWireS):
  Another USI I2C implementation, slave only
- [TinyWireM](https://github.com/adafruit/TinyWireM): Another USI I2C
  implementation, master only
- [AVR312: Using the USI Module as a I2C Slave, Application Note](http://www.atmel.com/ru/ru/Images/Atmel-2560-Using-the-USI-Module-as-a-I2C-Slave_ApplicationNote_AVR312.pdf)
- [AVR310: Using the USI module as a TWI Master, Application Note](http://www.atmel.com/images/atmel-2561-using-the-usi-module-as-a-i2c-master_ap-note_avr310.pdf)
