#ifndef __RESOURCES_H_
#define __RESOURCES_H_

#include <pgmspace.h>

static const char PARAM_VERSION[] PROGMEM = "version";
static const char PARAM_VERSION_ESP[] PROGMEM = "version_esp";
static const char PARAM_WATERIUS_HOST[] PROGMEM = "waterius_host";
static const char PARAM_WATERIUS_EMAIL[] PROGMEM = "waterius_email";
static const char PARAM_BLYNK_KEY[] PROGMEM = "blynk_key";
static const char PARAM_BLYNK_HOST[] PROGMEM = "blynk_host";
static const char PARAM_HTTP_URL[] PROGMEM = "http_url";
static const char PARAM_MQTT_HOST[] PROGMEM = "mqtt_host";
static const char PARAM_MQTT_PORT[] PROGMEM = "mqtt_port";
static const char PARAM_MQTT_LOGIN[] PROGMEM = "mqtt_login";
static const char PARAM_MQTT_PASSWORD[] PROGMEM = "mqtt_password";
static const char PARAM_MQTT_TOPIC[] PROGMEM = "mqtt_topic";
static const char PARAM_CHANNEL0_START[] PROGMEM = "channel0_start";
static const char PARAM_CHANNEL1_START[] PROGMEM = "channel1_start";
static const char PARAM_SERIAL0[] PROGMEM = "serial0";
static const char PARAM_SERIAL1[] PROGMEM = "serial1";
static const char PARAM_IP[] PROGMEM = "ip";
static const char PARAM_GATEWAY[] PROGMEM = "gateway";
static const char PARAM_MASK[] PROGMEM = "mask";
static const char PARAM_MAC_ADDRESS[] PROGMEM = "mac_address";
static const char PARAM_WAKEUP_PER_MIN[] PROGMEM = "wakeup_per_min";
static const char PARAM_MQTT_AUTO_DISCOVERY[] PROGMEM = "mqtt_auto_discovery";
static const char PARAM_MQTT_DISCOVERY_TOPIC[] PROGMEM = "mqtt_discovery_topic";
static const char PARAM_NTP_SERVER[] PROGMEM = "ntp_server";
static const char PARAM_SSID[] PROGMEM = "ssid";
static const char PARAM_PASSWORD[] PROGMEM = "password";
static const char PARAM_WIFI_PHY_MODE[] PROGMEM = "wifi_phy_mode";
static const char PARAM_COUNTER0_NAME[] PROGMEM = "counter0_name";
static const char PARAM_COUNTER1_NAME[] PROGMEM = "counter1_name";
static const char PARAM_COUNTER0_TITLE[] PROGMEM = "counter0_title";
static const char PARAM_COUNTER1_TITLE[] PROGMEM = "counter1_title";
static const char PARAM_COUNTER0_TYPE[] PROGMEM = "counter0_type";
static const char PARAM_COUNTER1_TYPE[] PROGMEM = "counter1_type";
static const char PARAM_COUNTER0_INSTRUCTION[] PROGMEM = "counter0_instruction";
static const char PARAM_COUNTER1_INSTRUCTION[] PROGMEM = "counter1_instruction";

static const char PARAM_FACTOR0[] PROGMEM = "factor0";
static const char PARAM_FACTOR1[] PROGMEM = "factor1";
static const char PARAM_WATERIUS_ON[] PROGMEM = "waterius_on";
static const char PARAM_HTTP_ON[] PROGMEM = "http_on";
static const char PARAM_MQTT_ON[] PROGMEM = "mqtt_on";
static const char PARAM_BLYNK_ON[] PROGMEM = "blynk_on";
static const char PARAM_DHCP_OFF[] PROGMEM = "dhcp_off";

static const char PARAM_WIZARD[] PROGMEM = "wizard";
static const char PARAM_TRUE[] PROGMEM = "true";

static const char ERROR_LENGTH_63[] PROGMEM = "Длина поля 63 символа";
static const char ERROR_VALUE[] PROGMEM = "Неверное значение";
static const char ERROR_EMPTY[] PROGMEM = "Значение не может быть пустым";

// logging
static const char PARAM_SAVED[] PROGMEM = "Saved: ";

#endif
