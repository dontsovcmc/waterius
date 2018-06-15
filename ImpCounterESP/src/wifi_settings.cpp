
#include "wifi_settings.h"
#include "Logging.h"

#include "Blynk/BlynkConfig.h"
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
		LOG_NOTICE( "WIF", "value0_start=" << sett.value0_start << ", impules0_start=" << sett.impules0_start << ", factor=" << sett.liters_per_impuls );
		LOG_NOTICE( "WIF", "value1_start=" << sett.value1_start << ", impules1_start=" << sett.impules1_start);
	}
}


/* At every boot the IP information is loaded from EEPROM */
bool loadConfig(struct Settings &sett) 
{
	EEPROM.begin( 512 );

	EEPROM.get(0, sett);

	if (sett.crc == FAKE_CRC) 
	{
		IPAddress ip(sett.ip);
		IPAddress subnet(sett.subnet);
		IPAddress gw(sett.gw);
		LOG_NOTICE( "WIF", "Config loaded: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString()  << ", hostname=" << sett.hostname);
		LOG_NOTICE( "WIF", "key=" << sett.key << " email=" << sett.email);
		LOG_NOTICE( "WIF", "value0_start=" << sett.value0_start << ", impules0_start=" << sett.impules0_start << ", factor=" << sett.liters_per_impuls );
		LOG_NOTICE( "WIF", "value1_start=" << sett.value1_start << ", impules1_start=" << sett.impules1_start);
		
		return true;
	}
	else 
	{
		LOG_NOTICE( "WIF", "crc failed=" << sett.crc );

		sett.version = CURRENT_VERSION;  //для совместимости в будущем
		sett.value0_start = 0.0;
		sett.value1_start = 0.0;
		sett.liters_per_impuls = 10;

		IPAddress ip(192, 168, 1, 116);
		sett.ip = ip;
		
		IPAddress subnet(255, 255, 255, 0);
		sett.subnet = subnet;

		IPAddress gw(192, 168, 1, 1);
		sett.gw = gw;
		
		String key = "";
		strncpy(sett.key, key.c_str(), KEY_LEN);

		String hostname = BLYNK_DEFAULT_DOMAIN;
		strncpy(sett.hostname, hostname.c_str(), HOSTNAME_LEN);

		String email = "";
		strncpy(sett.email, email.c_str(), EMAIL_LEN);

		LOG_NOTICE( "WIF", "Init config: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString() << ", hostname=" << hostname);
		return false;
	}
}