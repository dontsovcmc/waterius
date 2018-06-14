
#include "wifi_settings.h"
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
		LOG_NOTICE( "WIF", "key=" << sett.key);
		LOG_NOTICE( "WIF", "value0_start=" << sett.value0_start << ", impules0_start=" << sett.impules0_start << ", impules1=" << sett.impules1 << ", factor=" << sett.liters_per_impuls );
		LOG_NOTICE( "WIF", "value1_start=" << sett.value1_start << ", impules1_start=" << sett.impules1_start << ", impules1=" << sett.impules1);
	}
}


/* At every boot the IP information is loaded from EEPROM */
bool loadConfig(struct Settings &sett) 
{
	EEPROM.begin( 512 );

	EEPROM.get(0, sett);

	if (sett.crc == 1239) 
	{
		IPAddress ip(sett.ip);
		IPAddress subnet(sett.subnet);
		IPAddress gw(sett.gw);
		LOG_NOTICE( "WIF", "Config loaded: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString()  << ", hostname=" << sett.hostname);
		LOG_NOTICE( "WIF", "key=" << sett.key);
		LOG_NOTICE( "WIF", "value0_start=" << sett.value0_start << ", impules0_start=" << sett.impules0_start << ", impules1=" << sett.impules1 << ", factor=" << sett.liters_per_impuls );
		LOG_NOTICE( "WIF", "value1_start=" << sett.value1_start << ", impules1_start=" << sett.impules1_start << ", impules1=" << sett.impules1);
		
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

		sett.value0_start = 0.0;
		sett.value1_start = 0.0;
		sett.impules0 = 0; 
		sett.impules1 = 0;
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