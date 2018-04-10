#ifndef _SETUP_h
#define _SETUP_h

//#define LOGLEVEL 6

#define WIFI_TIMEOUT 3000UL // ms
#define I2C_SLAVE_ADDR 10

#define DEVICE_ID 12345
#define DEVICE_PWD 1234
#define AP_NAME "Impuls_Counter_12345"

//#define WEMOS
#define ESP_01
//#define NODEMCU

#ifdef ESP_01
    #define SETUP_PIN 3  //RX pin of ESP-01
    #define SDA_PIN 0
    #define SCL_PIN 2
    #define LED_PIN 4  //TX pin
#endif

#ifdef WEMOS
    #define SETUP_PIN RX   //3
    #define SDA_PIN SDA
    #define SCL_PIN SCL
#endif

#ifdef NODEMCU
    #define SETUP_PIN 3 //D5
    #define SDA_PIN 4 //SDA
    #define SCL_PIN 5 //SCL
#endif

#endif