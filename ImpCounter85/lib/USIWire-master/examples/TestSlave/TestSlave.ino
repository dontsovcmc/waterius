/*
 TestSlave.ino - TWI/I2C test slave from USIWire library
 Copyright (c) 2017 Puuu.  All right reserved.

 Simple slave implementation to test I2C communication with the
 TestMaster.  It should be run with any Arduino Wire compatible
 library.

 Connect SDA, SCL, AUX and GND of TestMaster and TestSlave.
*/

// Select Wire library fitting to your platform
#if defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) \
    || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny25__) \
    || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) \
    || defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny4313__) \
    || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny167__) \
    || defined( __AVR_ATtiny261__ ) || defined( __AVR_ATtiny461__ ) \
    || defined( __AVR_ATtiny861__ ) || defined(__AVR_ATtiny1634__)
#include <USIWire.h>
#else
#include <Wire.h>
#define PRINT Serial
#endif

// AVR sleep Modes
#include <avr/sleep.h>

// output pin
const uint8_t AUX_PIN = 4;

// Enable output to Serial if avaiable
//#define PRINT Serial

#ifdef PRINT
#define print(args...) PRINT.print(args)
#define println(args...) PRINT.println(args)
#else // save memeoy and do not compile strings
#define print(args...)
#define println(args...)
#endif

// Slave RX buffer size
#ifdef USIWire_h
const int SLAVE_BUFFER_SIZE = (BUFFER_LENGTH + 1) / 2 - 1;
#else
const int SLAVE_BUFFER_SIZE = BUFFER_LENGTH;
#endif
// slave register configuration (must be same on TestSlave and TestMaster))
#include "slave_register.h"

// register
uint8_t reg[REG_SIZE];

// current register address for read/write
volatile uint8_t addr = 0;

// current state of AUX part control register
inline uint8_t auxState() {
  return (((reg[CONTROL_ADDR]) >> CONTROL_AUX_POS) & CONTROL_AUX_MASK);
}

// current state of power part control register
inline uint8_t pwrState() {
  return (((reg[CONTROL_ADDR]) >> CONTROL_PWR_POS) & CONTROL_PWR_MASK);
}

// (re)initialize register
void resetReg(void) {
  for (uint8_t i = 0; i < REG_SIZE; i++) {
    if (i != CONTROL_ADDR) { // do not overwrite control byte
      reg[i] = REG_DEFAULT[i];
    }
  }
}


void setup() {
  pinMode(AUX_PIN, OUTPUT);
  resetReg(); //initialize register
  reg[CONTROL_ADDR] = 0x0; //initialize control byte
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

#ifdef PRINT
  PRINT.begin(9600);
  while(!PRINT); // for the Arduino Leonardo/Micro only
  println(F("USIWire TestSlave"));
#endif
}

void loop() {
  switch (auxState()) {
  case AUX_STATE_OFF:
    digitalWrite(AUX_PIN, LOW);
    break;
  case AUX_STATE_ON:
    digitalWrite(AUX_PIN, HIGH);
    break;
  case AUX_STATE_TOGGLE:
    digitalWrite(AUX_PIN, !digitalRead(AUX_PIN));
    break;
  }
  switch (pwrState()) {
  case PWR_STATE_AWAKE:
    break;
  case PWR_STATE_IDLE:
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_mode();
    // Arduino Timer0 (need for millis()) wakes us every 1 ms
    break;
  case PWR_STATE_DOWN:
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    #ifdef USIWire_h
    // power-down sleep mode is not possible during USI/TWI activity
    if (Wire.isActive()) set_sleep_mode(SLEEP_MODE_IDLE);
    #else
    // FIXME: Arduino Wire did not support power down sleep
    set_sleep_mode(SLEEP_MODE_IDLE);
    #endif
    sleep_mode();
    break;
  }
}

// function that executes whenever data is received from master
void receiveEvent(int howMany) {
  if (auxState() == AUX_STATE_CB) {
    // aux pin high after request, low after receive
    digitalWrite(AUX_PIN, LOW);
  }
  println();
  print('W');
  print(howMany);
  if (howMany <= 0) return;

  // read register address
  addr = Wire.read();
  if (addr & REG_ADDR_RST_FLAG_MASK) {
    //reset register
    resetReg();
  }
  // print addr
  print(F(" 0x"));
  print(addr, HEX);

  addr = (addr & REG_ADDR_MASK) % REG_SIZE;
  howMany--;

  // default to one requested byte
  uint8_t amount = 1; // amount of bytes expeted to read
  if (addr >= BLOCK_ADDR) {
    // SMBus block write: next byte is the amount of bytes
    if (howMany) {
      amount = Wire.read();
      howMany--;
      // print amount
      print(F(" 0x"));
      print(amount, HEX);
    }
  } else if (addr >= WORD_ADDR0) {
    // word write
    amount = 2;
  } else if (addr == ZERO_ADDR) {
    // do not write
    amount = 0;
  }
  // write the rest to the register, if this was a write request
  while (amount && howMany) {
    reg[addr] = Wire.read();
    if (amount == 1 || howMany == 1) {
      // print last byte
      print(F(" 0x"));
      print(reg[addr], HEX);
    }
    addr++; // set next addr
    if (addr >= REG_SIZE) addr = 0; // start at 0 on register end
    howMany--;
    amount--;
  }
  // clear rx buffer
  while (Wire.available()) Wire.read();
}

// function that executes whenever data is requested by master
void requestEvent() {
  if (auxState() == AUX_STATE_CB) {
    // aux pin high after request, low after receive
    digitalWrite(AUX_PIN, HIGH);
  }
  println();
  print('R');
  uint8_t amount = 1; // bytes to send
  if (addr >= BLOCK_ADDR) {
    // SMBus block read
    amount = BLOCK_RESP_LENGTH;
    // SMBus block read: first send the amount of bytes
    Wire.write(amount);
    // print amount
    print(F(" 0x"));
    print(amount, HEX);
  } else if (addr >= WORD_ADDR0) {
    // word read
    amount = 2;
  } else if (addr == ZERO_ADDR) {
    // do not read
    amount = 0;
    addr++; // set next addr
  }
  // print first byte
  print(F(" 0x"));
  print(reg[addr], HEX);
  // transmit the requested bytes
  while (amount) {
    Wire.write(reg[addr]);
    addr++; // set next addr
    if (addr >= REG_SIZE) addr = 0; // start at 0 on register end
    amount--;
  }
}