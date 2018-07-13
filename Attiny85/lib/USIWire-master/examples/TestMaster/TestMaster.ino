/*
 TestMaster.ino - TWI/I2C test master from USIWire library
 Copyright (c) 2017 Puuu.  All right reserved.

 Multiple test cases to test the I2C communication with the
 TestSlave.  It should be run with any Arduino Wire compatible
 library.

 Connect SDA, SCL, AUX and GND of TestMaster and TestSlave.

 You may have to adjust SLAVE_BUFFER_SZIE according to the TestSlave.

 Test status is printed to Serial (or whatever PRINT is defined for)
 and indicated by LED.  To disable status messages do not define
 PRINT.  Disable status messages may be necessary to run
 TestMaster on a ATTiny platform.
*/
// Select Wire library fitting to your platform
#if defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) \
    || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny25__) \
    || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) \
    || defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny4313__) \
    || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny167__) \
    || defined( __AVR_ATtiny261__ ) || defined( __AVR_ATtiny461__ ) \
    || defined( __AVR_ATtiny861__ ) || defined(__AVR_ATtiny1634__)
#define LED_BUILTIN 3
#include <USIWire.h>
#else
#include <Wire.h>
#define PRINT Serial
#endif

// input pin
const uint8_t AUX_PIN = 4;
// status LED
const uint8_t LED_PIN = LED_BUILTIN;

// Slave RX buffer size: USIWire: 15, Arduino Wire: 32
const int SLAVE_BUFFER_SIZE = 15;
// slave register configuration (must be same on TestSlave and TestMaster))
#include "slave_register.h"


// Default output to Serial
//#define PRINT Serial

#ifdef PRINT
#define print(args...) PRINT.print(args)
#define println(args...) PRINT.println(args)
#else // save memeoy and do not compile strings
#define print(args...)
#define println(args...)
#endif

// some test content
const uint8_t DATA1 = 0x0A;
const uint8_t DATA2 = 0x05;

// length constants
const int BYTE = 1;
const int WORD = 2;

// requested slave sleep mode
uint8_t slaveSleepMode = 0;

void setup() {
  pinMode(AUX_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

#ifdef PRINT
  PRINT.begin(9600);
  while(!PRINT); // for the Arduino Leonardo/Micro only
#endif

  Wire.begin();
#ifdef ESP8266  // default value of 230 us is too short
  Wire.setClockStretchLimit(1500);
#endif
  digitalWrite(LED_PIN, HIGH); // start with LED on
  runTests();
}

void loop() {
  // waste energy!
}

void runTests() {
  println(F("Sleep mode off!"));
  slaveSleepMode = (PWR_STATE_AWAKE << CONTROL_PWR_POS);
  allTests();
  println(F("Idle sleep mode!"));
  slaveSleepMode = (PWR_STATE_IDLE << CONTROL_PWR_POS);
  allTests();
  println(F("Power down sleep mode!"));
  slaveSleepMode = (PWR_STATE_DOWN << CONTROL_PWR_POS);
  allTests();
}

void allTests() {
  print(F("Initialize all tests: "));
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0);

  test_SMBus_Quick_Read_Command();
  test_SMBus_Quick_Write_Command();
  test_SMBus_Receive_Byte();
  test_SMBus_Send_Byte();
  test_SMBus_Read_Byte();
  test_SMBus_Read_Word();
  test_SMBus_Write_Byte();
  test_SMBus_Write_Word();
  test_SMBus_Process_Call();
  test_SMBus_Block_Read();
  test_SMBus_Block_Write();
  test_SMBus_Block_Process_Call();
  // Arduino Wire seems not to support broadcasts
  //test_SMBus_Quick_Command_Broadcast();
  //test_SMBus_Write_Byte_Broadcast();
  test_SMBus_Quick_Read_Command_Wrong_Addr();
  test_SMBus_Quick_Write_Command_Wrong_Addr();
  test_SMBus_Read_Byte_Multiple_Times();
  test_Read_Too_Much();
  test_Read_Too_Less();
  test_Write_Too_Much();
  test_Write_Too_Less();
  if (slaveSleepMode != (PWR_STATE_DOWN << CONTROL_PWR_POS)) {
    // skip this test in powerdown sleep mode
    test_AUX_On_Off();
  }
  test_AUX_On_Off_Repeated_Start();
  test_AUX_Callback();
  if (BUFFER_LENGTH > SLAVE_BUFFER_SIZE) {
    // These tests require a bigger master tx buffer than slave rx buffer
    test_Fill_Slave_RX_Buffer();
    test_AUX_On_Off_Repeated_Start_NACK();
    test_AUX_Callback_NACK();
    if (slaveSleepMode != (PWR_STATE_DOWN << CONTROL_PWR_POS)) {
      // skip this test in powerdown sleep mode
      test_AUX_On_Off_NACK();
    }
  }
  endTest();
  println(F("All test finished!"));
  println();
}

// test unit utilities

#ifndef PRINT // save memeoy and do not compile test names
#define startTest(str) startTest_()
void startTest_() {
#else
void startTest(const char* str) {
#endif
  endTest();
  print(str);
  print(F(": "));
  digitalWrite(LED_PIN, HIGH);
}

void assertEqual(int value1, int value2) {
  if (value1 == value2) {
    print('.');
  } else {
    println(F("FAILED!"));
    print(F("0x"));
    print(value1, HEX);
    print(F(" != 0x"));
    println(value2, HEX);
    errorState();
  }
}

void endTest() {
  digitalWrite(LED_PIN, LOW);
  println(F(" SUCCESS!"));
}

void errorState(void) {
  println(F("Error state!"));
  // signal error by blinking LED
  while (true) {
    digitalWrite(LED_PIN, LOW);
    delay(1000);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
  }
}


// *********** SMBus protocol

void test_SMBus_Quick_Read_Command() {
  startTest("SMBus_Quick_Read_Command");
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
}

void test_SMBus_Quick_Write_Command() {
  startTest("SMBus_Quick_Write_Command");
  Wire.beginTransmission(SLAVE_ADDR);
  assertEqual(Wire.endTransmission(), 0);
}

void test_SMBus_Receive_Byte() {
  startTest("SMBus_Receive_Byte");
  const int addr = BYTE_ADDR0;
  // set register addr and reset register
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(), 0);
  // read one byte
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.available(), 1);
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
}

void test_SMBus_Send_Byte() {
  startTest("SMBus_Send_Byte");
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(DATA1);
  assertEqual(Wire.endTransmission(), 0);
}

void test_SMBus_Read_Byte() {
  startTest("SMBus_Read_Byte");
  const int addr = BYTE_ADDR0;
  // read one byte from addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
}

void test_SMBus_Read_Word() {
  startTest("SMBus_Read_Word");
  const int addr = WORD_ADDR0;
  // read word from addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD);
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
  assertEqual(Wire.read(), REG_DEFAULT[addr+1]);
}

void test_SMBus_Write_Byte() {
  startTest("SMBus_Write_Byte");
  const int addr = BYTE_ADDR0;
  // Write one byte to addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(DATA1);
  assertEqual(Wire.endTransmission(), 0);
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), DATA1);
}

void test_SMBus_Write_Word() {
  startTest("SMBus_Write_Word");
  const int addr = WORD_ADDR0;
  // Write word to addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(DATA1);
  Wire.write(DATA2);
  assertEqual(Wire.endTransmission(), 0);
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD);
  assertEqual(Wire.read(), DATA1);
  assertEqual(Wire.read(), DATA2);
}

void test_SMBus_Process_Call() {
  startTest("SMBus_Process_Call");
  const int addr = WORD_ADDR0;
  // call addr with a word
  // slave will save the word and response with the next word in the register
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(DATA1);
  Wire.write(DATA2);
  assertEqual(Wire.endTransmission(false), 0);  // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD);
  assertEqual(Wire.read(), REG_DEFAULT[addr+2]);
  assertEqual(Wire.read(), REG_DEFAULT[addr+3]);
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD);
  assertEqual(Wire.read(), DATA1);
  assertEqual(Wire.read(), DATA2);
}

void test_SMBus_Block_Read() {
  startTest("SMBus_Block_Read");
  const int addr = BLOCK_ADDR;
  const int amount = min(BLOCK_RESP_LENGTH, REG_SIZE-BLOCK_ADDR);
  // read data in block mode
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, amount+1), amount+1);
  assertEqual(Wire.read(), BLOCK_RESP_LENGTH); // first byte is the amount
  // validate the bytes
  for (int i = 0; i < amount; i++) {
    assertEqual(Wire.read(), REG_DEFAULT[addr + i]);
  }
}

void test_SMBus_Block_Write() {
  startTest("SMBus_Block_Write");
  const int addr = BLOCK_ADDR;
  const int amount = min(BLOCK_RESP_LENGTH-1, REG_SIZE-BLOCK_ADDR);
  // write data in block mode
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(amount);
  for (uint8_t i = 0; i < amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(), 0);
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, amount+1), amount+1);
  assertEqual(Wire.read(), BLOCK_RESP_LENGTH); // first byte is the amount
  for (uint8_t i = 0; i < amount; i++) {
    assertEqual(Wire.read(), i);
  }
}

void test_SMBus_Block_Process_Call() {
  startTest("SMBus_Block_Process_Call");
  const int addr = BLOCK_ADDR;
  const int write_amount = 4;
  const int read_amount = min(BLOCK_RESP_LENGTH,
                              REG_SIZE - (BLOCK_ADDR + write_amount));
  // call addr with block mode
  // slave will save the data and response with the next data in the register
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(write_amount);
  for (uint8_t i = 0; i < write_amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(false), 0);  // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, read_amount+1),
              read_amount+1);
  assertEqual(Wire.read(), BLOCK_RESP_LENGTH); // first byte is the amount
  for (int i = 0; i < read_amount; i++) {
    assertEqual(Wire.read(), REG_DEFAULT[addr + write_amount + i]);
  }
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, write_amount+1), write_amount+1);
  assertEqual(Wire.read(), BLOCK_RESP_LENGTH); // first byte is the amount
  for (uint8_t i = 0; i < write_amount; i++) {
    assertEqual(Wire.read(), i);
  }
}

// *********** Broadcast

void test_SMBus_Quick_Command_Broadcast() {
  startTest("SMBus_Quick_Command_Broadcast");
  Wire.beginTransmission(0x0);
  assertEqual(Wire.endTransmission(), 0);
}

void test_SMBus_Write_Byte_Broadcast() {
  startTest("SMBus_Write_Byte_Broadcast");
  const int addr = BYTE_ADDR0;
  // Write one byte to 0 (broadcast addr)
  Wire.beginTransmission(0x0);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(DATA1);
  assertEqual(Wire.endTransmission(), 0);
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), DATA1);
}

// *********** Wrong Addresses

void test_SMBus_Quick_Read_Command_Wrong_Addr() {
  startTest("SMBus_Quick_Read_Command_Wrong_Addr");
  // error code 2: received NACK on transmit of address
  Wire.beginTransmission(SLAVE_ADDR-1);
  assertEqual(Wire.endTransmission(), 2);
  Wire.beginTransmission(SLAVE_ADDR+1);
  assertEqual(Wire.endTransmission(), 2);
  Wire.beginTransmission(0xAA);
  assertEqual(Wire.endTransmission(), 2);
  Wire.beginTransmission(0x55);
  assertEqual(Wire.endTransmission(), 2);
  Wire.beginTransmission(0x7F);
  assertEqual(Wire.endTransmission(), 2);
}

void test_SMBus_Quick_Write_Command_Wrong_Addr() {
  startTest("SMBus_Quick_Write_Command_Wrong_Addr");
  assertEqual(Wire.requestFrom(SLAVE_ADDR-1, 1), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR+1, 1), 0);
  assertEqual(Wire.requestFrom(0xAA, 1), 0);
  assertEqual(Wire.requestFrom(0x55, 1), 0);
  assertEqual(Wire.requestFrom(0x7F, 1), 0);
}

// *********** Validations

void test_SMBus_Read_Byte_Multiple_Times() {
  startTest("SMBus_Read_Byte_Multiple_Times");
  const int addr = BYTE_ADDR1;
  // read one byte from addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
  // read one byte from addr more 5 times
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(addr);
    assertEqual(Wire.endTransmission(false), 0); // no stop condition
    assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
    assertEqual(Wire.read(), REG_DEFAULT[addr]);
  }
}

void test_Read_Too_Much() {
  startTest("Read_Too_Much");
  const int addr = BYTE_ADDR0;
  // read two byte from byte addr (slave only send one)
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD); // request 2 byte
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
  // if slave is not writing, then SDA is always high
  assertEqual(Wire.read(), 0xFF);
}

void test_Read_Too_Less() {
  startTest("Read_Too_Less");
  const int addr = WORD_ADDR0;
  // read only one byte from word addr (slave send two)
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE); // request only 1 byte
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
  // repeat this 5 times
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(addr);
    assertEqual(Wire.endTransmission(false), 0); // no stop condition
    assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE); //request only 1 byte
    assertEqual(Wire.read(), REG_DEFAULT[addr]);
  }
  // now read two byte
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD); // request 2 byte
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
  assertEqual(Wire.read(), REG_DEFAULT[addr+1]);
}

void test_Write_Too_Much() {
  startTest("Write_Too_Much");
  const int addr = BYTE_ADDR0;
  // write two byte to byte addr (slave accepts only one)
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(DATA1);
  Wire.write(DATA2);
  assertEqual(Wire.endTransmission(), 0);
  // repeat this 5 times
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(addr);
    Wire.write(DATA1);
    Wire.write(DATA2);
    assertEqual(Wire.endTransmission(), 0);
  }
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), DATA1);
  // check following data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr+1);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), REG_DEFAULT[addr+1]); // not DATA2
}

void test_Write_Too_Less() {
  startTest("Write_Too_Less");
  const int addr = WORD_ADDR0;
  // write only one byte to word addr (slave wants two)
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(DATA1);
  assertEqual(Wire.endTransmission(), 0);
  // repeat this 5 times
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(addr);
    Wire.write(DATA1);
    assertEqual(Wire.endTransmission(), 0);
  }
  // check data on slave
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr);
  assertEqual(Wire.endTransmission(false), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, WORD), WORD);
  assertEqual(Wire.read(), DATA1);
  assertEqual(Wire.read(), REG_DEFAULT[addr+1]);
}

void test_Fill_Slave_RX_Buffer() {
  startTest("Fill_Slave_RX_Buffer");
  const int addr = BLOCK_ADDR;
  // Write 5 bytes more than slave RX buffer size
  const int amount = SLAVE_BUFFER_SIZE + 5;
  // write data in block mode
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  Wire.write(amount);
  for (uint8_t i = 0; i < amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(), 3); //received NACK on transmit of data
  // repeat this 2 times
  for (int j = 0; j < 2; j++) {
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(addr);
    Wire.write(amount);
    for (uint8_t i = 0; i < amount; i++) {
      Wire.write(i);
    }
    assertEqual(Wire.endTransmission(), 3); //received NACK on transmit of data
  }
  // check data on slave
  for (int i = 0; i < (REG_SIZE/BLOCK_RESP_LENGTH+1); i++) { // read all
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(addr);
    assertEqual(Wire.endTransmission(false), 0);
    assertEqual(Wire.requestFrom(SLAVE_ADDR, BLOCK_RESP_LENGTH+1),
                BLOCK_RESP_LENGTH+1);
    assertEqual(Wire.read(), BLOCK_RESP_LENGTH); // first byte is the amount
    for (uint8_t j = 0; j < BLOCK_RESP_LENGTH; j++) {
      int tmpAddr = (addr + i * BLOCK_RESP_LENGTH + j) % REG_SIZE;
      if (i == 0 && j < BLOCK_RESP_LENGTH-1) { // changed data
        assertEqual(Wire.read(), j);
      } else if (tmpAddr >= addr) {
        break; // one round finish
      } else if (tmpAddr != CONTROL_ADDR) { // do not check CONTROL byte
        assertEqual(Wire.read(), REG_DEFAULT[tmpAddr]);
      }
    }
  }
}

void test_Slave_Zero_Write() {
  startTest("Slave_Zero_Write");
  // try to read one byte from zero addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(ZERO_ADDR | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), 0xFF); // slave do not response, SDA always high
  // test normal read still working
  const int addr = BYTE_ADDR0;
  // read one byte from addr
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(addr | REG_ADDR_RST_FLAG_MASK);
  assertEqual(Wire.endTransmission(false), 0); // no stop condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(Wire.read(), REG_DEFAULT[addr]);
}

// *********** reaction utilizing AUX pin

void test_AUX_On_Off() {
  startTest("AUX_On_Off");
  // cycle AUX pin 5 times
  for (int i = 0; i < 5; i++) {
    // AUX pin high
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_ON << CONTROL_AUX_POS | slaveSleepMode);
    assertEqual(Wire.endTransmission(), 0);
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), HIGH);
    // AUX pin low
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
    assertEqual(Wire.endTransmission(), 0);
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), LOW);
  }
}

void test_AUX_On_Off_NACK() {
  startTest("AUX_On_Off_NACK");
  // Write 5 bytes more than slave RX buffer size
  const int amount = SLAVE_BUFFER_SIZE + 5;
  // cycle AUX pin 5 times
  for (int i = 0; i < 5; i++) {
    // AUX pin high
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_ON << CONTROL_AUX_POS | slaveSleepMode);
    for (uint8_t i = 0; i < amount; i++) {
      Wire.write(i);
    }
    assertEqual(Wire.endTransmission(), 3);
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), HIGH);
    // AUX pin low
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
    for (uint8_t i = 0; i < amount; i++) {
      Wire.write(i);
    }
    assertEqual(Wire.endTransmission(), 3);
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), LOW);
  }
}

void test_AUX_On_Off_Repeated_Start() {
  startTest("AUX_On_Off_Repeated_Start");
  // AUX pin to low
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0); // send no Stop Condition
  // cycle AUX pin 5 times
  for (int i = 0; i < 5; i++) {
    // request AUX pin high
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_ON << CONTROL_AUX_POS | slaveSleepMode);
    assertEqual(Wire.endTransmission(false), 0); // send no Stop Condition
    assertEqual(digitalRead(AUX_PIN), LOW); // still low
    Wire.beginTransmission(SLAVE_ADDR);
    assertEqual(Wire.endTransmission(false), 0); // repeat start
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), HIGH);
    // request AUX pin low
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
    assertEqual(Wire.endTransmission(false), 0); // send no Stop Condition
    assertEqual(digitalRead(AUX_PIN), HIGH); // still high
    Wire.beginTransmission(SLAVE_ADDR);
    assertEqual(Wire.endTransmission(false), 0); // repeat start
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), LOW);
  }
  Wire.beginTransmission(SLAVE_ADDR);
  assertEqual(Wire.endTransmission(true), 0); // send Stop Condition
  delay(1);
  if (slaveSleepMode) delay(2);
  assertEqual(digitalRead(AUX_PIN), LOW);
}

void test_AUX_On_Off_Repeated_Start_NACK() {
  startTest("AUX_On_Off_Repeated_Start_NACK");
  // Write 5 bytes more than slave RX buffer size
  const int amount = SLAVE_BUFFER_SIZE + 5;
  // AUX pin to low
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0); // send no Stop Condition
  // cycle AUX pin 5 times
  for (int i = 0; i < 5; i++) {
    // request AUX pin high
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_ON << CONTROL_AUX_POS | slaveSleepMode);
    for (uint8_t i = 0; i < amount; i++) {
      Wire.write(i);
    }
    assertEqual(Wire.endTransmission(false), 3); // send no Stop Condition
    assertEqual(digitalRead(AUX_PIN), LOW); // still low
    Wire.beginTransmission(SLAVE_ADDR);
    assertEqual(Wire.endTransmission(false), 0); // repeat start
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), HIGH);
    // request AUX pin low
    Wire.beginTransmission(SLAVE_ADDR);
    Wire.write(CONTROL_ADDR);
    Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
    for (uint8_t i = 0; i < amount; i++) {
      Wire.write(i);
    }
    assertEqual(Wire.endTransmission(false), 3); // send no Stop Condition
    assertEqual(digitalRead(AUX_PIN), HIGH); // still high
    Wire.beginTransmission(SLAVE_ADDR);
    assertEqual(Wire.endTransmission(false), 0); // repeat start
    delay(1);
    if (slaveSleepMode) delay(2);
    assertEqual(digitalRead(AUX_PIN), LOW);
  }
  Wire.beginTransmission(SLAVE_ADDR);
  assertEqual(Wire.endTransmission(true), 0); // send Stop Condition
  delay(1);
  if (slaveSleepMode) delay(2);
  assertEqual(digitalRead(AUX_PIN), LOW);
}

void test_AUX_Callback() {
  startTest("AUX_Callback");
  // activate AUX pin by callback
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(AUX_STATE_CB << CONTROL_AUX_POS | slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0); // Stop condition
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  // simple read
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(digitalRead(AUX_PIN), HIGH); // high after read event
  // simple write
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(BYTE_ADDR0);
  Wire.write(DATA1);
  assertEqual(Wire.endTransmission(), 0);
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  // read zero
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(ZERO_ADDR);
  assertEqual(Wire.endTransmission(), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(digitalRead(AUX_PIN), HIGH); // high after read event
  // write zero
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(ZERO_ADDR);
  assertEqual(Wire.endTransmission(), 0);
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  //repeated start, read
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(BYTE_ADDR0);
  assertEqual(Wire.endTransmission(false), 0); // no Stop Condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE, 0),
              BYTE);  // no Stop Condition
  assertEqual(digitalRead(AUX_PIN), HIGH); // high after read event
#ifndef USIWire_h
  // Workaround for Arduino Wire
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.endTransmission(false); // no Stop Condition
#endif
  //repeated start, write
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(BYTE_ADDR0);
  Wire.write(DATA1);
  assertEqual(Wire.endTransmission(false), 0); // no Stop Condition
  assertEqual(digitalRead(AUX_PIN), HIGH); // still high
  Wire.beginTransmission(SLAVE_ADDR);
  assertEqual(Wire.endTransmission(false), 0); // repeat start
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  // AUX off
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0);
}

void test_AUX_Callback_NACK() {
  startTest("AUX_Callback_NACK");
  // Write 5 bytes more than slave RX buffer size
  const int amount = SLAVE_BUFFER_SIZE + 5;
  // activate AUX pin by callback
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(AUX_STATE_CB << CONTROL_AUX_POS | slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0); // Stop condition
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  // simple read
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE+1), BYTE+1);
  assertEqual(digitalRead(AUX_PIN), HIGH); // high after read event
  // simple write
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(BYTE_ADDR0);
  Wire.write(DATA1);
  for (uint8_t i = 0; i < amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(), 3);
  delayMicroseconds(100);
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  // read zero
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(ZERO_ADDR);
  assertEqual(Wire.endTransmission(), 0);
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE), BYTE);
  assertEqual(digitalRead(AUX_PIN), HIGH); // high after read event
  // write zero
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(ZERO_ADDR);
  for (uint8_t i = 0; i < amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(), 3);
  delayMicroseconds(100);
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  //repeated start, read
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(BYTE_ADDR0);
  for (uint8_t i = 0; i < amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(false), 3); // no Stop Condition
  assertEqual(Wire.requestFrom(SLAVE_ADDR, BYTE, 0),
              BYTE);  // no Stop Condition
  assertEqual(digitalRead(AUX_PIN), HIGH); // high after read event
  //repeated start, write
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(BYTE_ADDR0);
  Wire.write(DATA1);
  for (uint8_t i = 0; i < amount; i++) {
    Wire.write(i);
  }
  assertEqual(Wire.endTransmission(false), 3); // no Stop Condition
  Wire.beginTransmission(SLAVE_ADDR);
  assertEqual(Wire.endTransmission(false), 0); // repeat start
  assertEqual(digitalRead(AUX_PIN), LOW); // low after write event
  // AUX off
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(CONTROL_ADDR);
  Wire.write(AUX_STATE_OFF << CONTROL_AUX_POS | slaveSleepMode);
  assertEqual(Wire.endTransmission(), 0);
}