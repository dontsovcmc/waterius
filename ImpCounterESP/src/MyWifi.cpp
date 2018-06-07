#include "MyWifi.h"
#include "Logging.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>		  // Configuration portal.

#include <EEPROM.h>

#define AP_NAME "ImpulsCounter_0.3"

WiFiClient client;

void MyWifi::setup(Settings &sett) 
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

	sett.crc = 1234;
	storeConfig(sett);
}

bool MyWifi::connect(const Settings &sett)
{
	LOG_NOTICE( "ESP", "I2C-begined: mode TRANSMIT" );
	LOG_NOTICE( "WIF", "Starting Wifi" );
	IPAddress ip(sett.ip);
	IPAddress gw(sett.gw);
	IPAddress subnet(sett.subnet);
	WiFi.config( ip, gw, subnet );
	WiFi.begin();

	uint32_t now = millis();
	while ( WiFi.status() != WL_CONNECTED && millis() - now < ESP_CONNECT_TIMEOUT)
	{
		LOG_NOTICE( "WIF", "Wifi status: " << WiFi.status());
		delay(100);
	}

	if (millis() - now > ESP_CONNECT_TIMEOUT)
	{
		LOG_ERROR( "WIF", "Wifi connect error");
		return false;
	}
	else
	{
		LOG_NOTICE( "WIF", "Wifi connected, got IP address: " << WiFi.localIP().toString() );
	}
	return true;
}


/* Sends provided data to server */
bool MyWifi::send(const Settings &sett, const void * data, uint16_t length )
{
	LOG_NOTICE( "WIF", "Making TCP connection to " << sett.hostname );

	client.setTimeout( SERVER_TIMEOUT ); 
	if ( !client.connect( IPAddress(sett.server), 4001 ) ) {
		LOG_ERROR( "WIF", "Connection to server failed" );
		return false;
	}

	LOG_NOTICE( "WIF", "Sending " << length << " bytes of data" );
	uint16_t bytesSent = client.write( (char*) data, length );

	client.stop();
	if ( bytesSent == length ) {
		LOG_NOTICE( "WIF", "Data sent successfully" );
		return true;
	} else {
		LOG_ERROR( "WIF", "Could not send data" );
		return false;
	}
}


/* Takes all the IP information we got from Wifimanger and saves it in EEPROM */
void storeConfig(const Settings &sett) 
{
	EEPROM.begin( 512 );

	EEPROM.put(0, sett);
	
	if (!EEPROM.commit())
		LOG_ERROR("WIF", "Config stored FAILED");
	else
	{
		IPAddress ip(sett.ip);
		IPAddress subnet(sett.subnet);
		IPAddress gw(sett.gw);
		LOG_NOTICE( "WIF", "Config stored: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString() << ", hostname=" << sett.hostname );
	}
}


/* At every boot the IP information is loaded from EEPROM */
bool loadConfig(struct Settings &sett) 
{
	EEPROM.begin( 512 );

	EEPROM.get(0, sett);

	if (sett.crc == 1234) 
	{
		IPAddress ip(sett.ip);
		IPAddress subnet(sett.subnet);
		IPAddress gw(sett.gw);
		LOG_NOTICE( "WIF", "Config loaded: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString()  << ", hostname=" << sett.hostname);
		LOG_NOTICE( "WIF", "SSID=" << WiFi.SSID().c_str() << "psk=" << WiFi.psk().c_str()) << "key=" << sett.key);
		return true;
	}
	else 
	{
		LOG_NOTICE( "WIF", "crc failed=" << sett.crc );
		IPAddress ip(192, 168, 1, 73);
		IPAddress subnet(255, 255, 255, 0);
		IPAddress gw(192, 168, 1, 1);
		IPAddress server(0,0,0,0);
		String hostname = "blynk-cloud.com";
		String key = "";


		sett.litres0_start = 0.0;
		sett.litres1_start = 0.0;
		sett.liters_per_impuls = 10;

		sett.ip = ip;
		sett.subnet = subnet;
		sett.gw = gw;
		sett.server = server;
		strncpy(sett.key, key.c_str(), KEY_LEN);
		strncpy(sett.hostname, hostname.c_str(), HOSTNAME_LEN);
		LOG_NOTICE( "WIF", "Init config: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString() << ", hostname=" << hostname);
		return false;
	}
}