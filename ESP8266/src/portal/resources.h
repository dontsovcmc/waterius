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
static const char PARAM_WIFI_SSID[] PROGMEM = "wifi_ssid";
static const char PARAM_WIFI_PASSWORD[] PROGMEM = "wifi_password";
static const char PARAM_WIFI_PHY_MODE[] PROGMEM = "wifi_phy_mode";
static const char PARAM_COUNTER0_NAME[] PROGMEM = "counter0_name";
static const char PARAM_COUNTER1_NAME[] PROGMEM = "counter1_name";
static const char PARAM_COUNTER0_TYPE[] PROGMEM = "counter0_type";
static const char PARAM_COUNTER1_TYPE[] PROGMEM = "counter1_type";
static const char PARAM_FACTOR0[] PROGMEM = "factor0";
static const char PARAM_FACTOR1[] PROGMEM = "factor1";
static const char PARAM_WATERIUS_ON[] PROGMEM = "waterius_on";
static const char PARAM_HTTP_ON[] PROGMEM = "http_on";
static const char PARAM_MQTT_ON[] PROGMEM = "mqtt_on";
static const char PARAM_BLYNK_ON[] PROGMEM = "blynk_on";
static const char PARAM_DHCP_ON[] PROGMEM = "dhcp_on";

static const char PARAM_WMAIL[] PROGMEM = "wmail";
static const char PARAM_WHOST[] PROGMEM = "whost";
static const char PARAM_MPERIOD[] PROGMEM = "mperiod";
static const char PARAM_BHOST[] PROGMEM = "bhost";
static const char PARAM_BKEY[] PROGMEM = "bkey";
static const char PARAM_BMAIL[] PROGMEM = "bemail";
static const char PARAM_BTITLE[] PROGMEM = "btitle";
static const char PARAM_BTEMPLATE[] PROGMEM = "btemplate";
static const char PARAM_MHOST[] PROGMEM = "mhost";
static const char PARAM_MPORT[] PROGMEM = "mport";
static const char PARAM_MLOGIN[] PROGMEM = "mlogin";
static const char PARAM_MPASSWORD[] PROGMEM = "mpassword";
static const char PARAM_MTOPIC[] PROGMEM = "mtopic";
static const char PARAM_MDAUTO[] PROGMEM = "auto_discovery_checkbox";
static const char PARAM_MDTOPIC[] PROGMEM = "discovery_topic";
static const char PARAM_MAC[] PROGMEM = "mac";
static const char PARAM_GW[] PROGMEM = "gw";
static const char PARAM_SN[] PROGMEM = "sn";
static const char PARAM_NTP[] PROGMEM = "ntp";
static const char PARAM_FACTORCOLD[] PROGMEM = "factorCold";
static const char PARAM_FACTORHOT[] PROGMEM = "factorHot";
static const char PARAM_SERIALCOLD[] PROGMEM = "serialCold";
static const char PARAM_SERIALHOT[] PROGMEM = "serialHot";
static const char PARAM_CNAMEHOT[] PROGMEM = "counter0_name";
static const char PARAM_CNAMECOLD[] PROGMEM = "counter1_name";
static const char PARAM_CTYPEHOT[] PROGMEM = "counter_type0";
static const char PARAM_CTYPECOLD[] PROGMEM = "counter_type1";
static const char PARAM_CH0[] PROGMEM = "ch0";
static const char PARAM_CH1[] PROGMEM = "ch1";
static const char PARAM_S[] PROGMEM = "s";
static const char PARAM_P[] PROGMEM = "p";

static const char PARAM_ELAPSED[] PROGMEM = "elapsed";
static const char PARAM_ERROR[] PROGMEM = "error";
static const char TEXT_CONNECTERROR[] PROGMEM = "Ошибка связи с МК";

static const char ERROR_LENGTH_63[] PROGMEM = "Длина поля 63 символа";
static const char ERROR_VALUE[] PROGMEM = "Неверное значение";
static const char PARAM_SAVED[] PROGMEM = "Saved: ";

#endif