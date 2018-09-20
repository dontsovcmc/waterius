
#include "setup_ap.h"
#include "Logging.h"
#include "wifi_settings.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiClient.h>
#include <EEPROM.h>


#define AP_NAME "Waterius_0.4.3"

void print_hex(const char *title, char *buf, const uint16_t len)
{
	char *p = buf;
	uint16_t i = len;
	char hex[3];
	Serial.print(title);
	Serial.print(':');
	Serial.print(' ');
	while (*p && i > 0) {
		snprintf(hex, 3, "%02x", *p);
		p++;
		i--;
		Serial.print(hex);
		Serial.print(' ');
	}
	Serial.println();
}

void setup_ap(Settings &sett, const SlaveData &data, const float &value0, const float &value1) 
{
	// Пользователь нажал кнопку - запускаем веб сервер
	LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
	WiFi.disconnect( true );
	WiFiManager wifiManager;
	LOG_NOTICE( "WIF", "User requested captive portal" );
	
	LOG_NOTICE( "WIF", "Print hex" );
	print_hex("key", sett.key, KEY_LEN);
	print_hex("host", sett.hostname, HOSTNAME_LEN);
	print_hex("email", sett.email, EMAIL_LEN);
	print_hex("title", sett.email_title, EMAIL_TITLE_LEN);
	print_hex("template", sett.email_template, EMAIL_TEMPLATE_LEN);
	print_hex("value0", (char*)&value0, 4);
	print_hex("value1", (char*)&value1, 4);
	print_hex("factor", (char*)&sett.liters_per_impuls, 4);

	LOG_NOTICE( "WIF", "Print ok" );

	WiFiManagerParameter param_key( "key", "key",  sett.key, KEY_LEN );
	wifiManager.addParameter( &param_key );
	LOG_NOTICE( "WIF", "key ok" );
	
	WiFiManagerParameter param_hostname( "host", "host",  sett.hostname, HOSTNAME_LEN );
	wifiManager.addParameter( &param_hostname );
	LOG_NOTICE( "WIF", "host ok" );

	WiFiManagerParameter param_email( "email", "email",  sett.email, EMAIL_LEN );
	wifiManager.addParameter( &param_email );
	LOG_NOTICE( "WIF", "email ok" );
	
	WiFiManagerParameter param_email_title( "title", "title",  sett.email_title, EMAIL_TITLE_LEN );
	wifiManager.addParameter( &param_email_title );
	LOG_NOTICE( "WIF", "title ok" );
	
	WiFiManagerParameter param_email_template( "template", "template",  sett.email_template, EMAIL_TEMPLATE_LEN );
	wifiManager.addParameter( &param_email_template );
	LOG_NOTICE( "WIF", "template ok" );

	FloatParameter param_value0_start( "value0", "value0",  value0);
	wifiManager.addParameter( &param_value0_start );
	FloatParameter param_value1_start( "value1", "value1",  value1);
	wifiManager.addParameter( &param_value1_start );

	LongParameter param_litres_per_imp( "factor", "factor",  sett.liters_per_impuls);
	wifiManager.addParameter( &param_litres_per_imp );

	wifiManager.setConfigPortalTimeout(300);
	wifiManager.setConnectTimeout(15);
	
	LOG_NOTICE( "WIF", "start config portal" );

	// Start the portal with the SSID 
	wifiManager.startConfigPortal( AP_NAME );

	//

	LOG_NOTICE( "WIF", "Connected to wifi" );

	// Get all the values that user entered in the portal and save it in EEPROM

	/*sett.ip = param_ip.getValue();
	sett.subnet = param_subnet.getValue();
	sett.gw = param_gw.getValue();*/
	
	strncpy(sett.key, param_key.getValue(), KEY_LEN);
	strncpy(sett.hostname, param_hostname.getValue(), HOSTNAME_LEN);
	strncpy(sett.email, param_email.getValue(), EMAIL_LEN);
	strncpy(sett.email_title, param_email_title.getValue(), EMAIL_TITLE_LEN);
	strncpy(sett.email_template, param_email_template.getValue(), EMAIL_TEMPLATE_LEN);

	sett.value0_start = param_value0_start.getValue();
	sett.value1_start = param_value1_start.getValue();
	
	sett.prev_value0 = sett.value0_start;
	sett.prev_value1 = sett.value1_start;

	sett.liters_per_impuls = param_litres_per_imp.getValue();

	sett.impules0_start = data.impulses0;
	sett.impules1_start = data.impulses1;

	LOG_NOTICE( "DAT", "impulses0_start=data.impulses0=" << sett.impules0_start );
	LOG_NOTICE( "DAT", "impulses1_start=data.impulses1=" << sett.impules1_start );


	LOG_NOTICE( "WIF", "Print hex" );
	
	print_hex("key", sett.key, KEY_LEN);
	print_hex("host", sett.hostname, HOSTNAME_LEN);
	print_hex("email", sett.email, EMAIL_LEN);
	print_hex("title", sett.email_title, EMAIL_TITLE_LEN);
	print_hex("template", sett.email_template, EMAIL_TEMPLATE_LEN);
	print_hex("impules0_start", (char*)&sett.impules0_start, 4);
	print_hex("impules1_start", (char*)&sett.impules1_start, 4);
	print_hex("factor", (char*)&sett.liters_per_impuls, 4);

	LOG_NOTICE( "WIF", "Print ok" );

	sett.crc = FAKE_CRC;
	storeConfig(sett);
}
