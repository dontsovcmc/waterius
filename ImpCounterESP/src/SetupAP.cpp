#include "WifiSettings.h"
#include "Logging.h"
#include <ESP8266WiFi.h>

#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>		  // Configuration portal.
#include "MasterI2C.h"

#include <EEPROM.h>

#define AP_NAME "ImpulsCounter_0.3"


void setup_ap(Settings &sett, const SlaveData &data) 
{
	// Пользователь нажал кнопку - запускаем веб сервер
	LOG_NOTICE( "ESP", "I2C-begined: mode SETUP" );
	LOG_NOTICE( "WIF", "User requested captive portal" );
	WiFi.disconnect( true );
	WiFiManager wifiManager;
	
	WiFiManagerParameter param_ip( "IP", "Static IP", IPAddress(sett.ip).toString().c_str(), 20 );
	wifiManager.addParameter( &param_ip );
	WiFiManagerParameter param_subnet( "Subnet", "Subnet",  IPAddress(sett.subnet).toString().c_str(), 20 );
	wifiManager.addParameter( &param_subnet );
	WiFiManagerParameter param_gw( "GW", "Gateway",  IPAddress(sett.gw).toString().c_str(), 20 );
	wifiManager.addParameter( &param_gw );

	WiFiManagerParameter param_key( "Key", "Key",  sett.key, KEY_LEN );
	wifiManager.addParameter( &param_key );
	WiFiManagerParameter param_hostname( "Host", "Host",  sett.hostname, HOSTNAME_LEN );
	wifiManager.addParameter( &param_hostname );

	WiFiManagerParameter param_litres0_start( "Litres0", "Litres0",  String(sett.litres0_start).c_str(), 7 );
	wifiManager.addParameter( &param_litres0_start );
	WiFiManagerParameter param_litres1_start( "Litres1", "Litres1",  String(sett.litres1_start).c_str(), 7 );
	wifiManager.addParameter( &param_litres1_start );

	WiFiManagerParameter param_litres_per_imp( "Litres", "Litres_impuls",  String(sett.liters_per_impuls).c_str(), 5 );
	wifiManager.addParameter( &param_litres_per_imp );

	// Start the portal with the SSID 
	wifiManager.startConfigPortal( AP_NAME );
	LOG_NOTICE( "WIF", "Connected to wifi" );

	// Get all the values that user entered in the portal and save it in EEPROM
	IPAddress ip;
	ip.fromString( param_ip.getValue() );
	sett.ip = ip;
	ip.fromString( param_subnet.getValue() );
	sett.subnet = ip;
	ip.fromString( param_gw.getValue() );
	sett.gw = ip;
	
	strncpy(sett.key, param_key.getValue(), KEY_LEN);
	strncpy(sett.hostname, param_hostname.getValue(), HOSTNAME_LEN);

	sett.litres0_start = String(param_litres0_start.getValue()).toFloat();
	sett.litres1_start = String(param_litres1_start.getValue()).toFloat();
	sett.liters_per_impuls = String(param_litres_per_imp.getValue()).toInt();

	sett.impules0_start = data.value0;
	sett.impules1_start = data.value1;

	LOG_NOTICE( "DAT", "impulses0_start=data.value0=" << sett.impules0_start );
	LOG_NOTICE( "DAT", "impulses1_start=data.value1=" << sett.impules1_start );

	sett.crc = 1234;
	storeConfig(sett);
}

