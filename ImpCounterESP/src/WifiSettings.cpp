
#include "WifiSettings.h"
#include "Logging.h"

#include <IPAddress.h>
#include <EEPROM.h>

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
		LOG_NOTICE( "WIF", "key=" << sett.key);
		return true;
	}
	else 
	{
		LOG_NOTICE( "WIF", "crc failed=" << sett.crc );
		IPAddress ip(192, 168, 1, 116);
		IPAddress subnet(255, 255, 255, 0);
		IPAddress gw(192, 168, 1, 1);
		String hostname = "blynk-cloud.com";
		String key = "";


		sett.litres0_start = 0.0;
		sett.litres1_start = 0.0;
		sett.liters_per_impuls = 10;

		sett.ip = ip;
		sett.subnet = subnet;
		sett.gw = gw;
		strncpy(sett.key, key.c_str(), KEY_LEN);
		strncpy(sett.hostname, hostname.c_str(), HOSTNAME_LEN);
		LOG_NOTICE( "WIF", "Init config: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString() << ", hostname=" << hostname);
		return false;
	}
}