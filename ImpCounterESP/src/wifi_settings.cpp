
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
		/*IPAddress ip(sett.ip);
		IPAddress subnet(sett.subnet);
		IPAddress gw(sett.gw);
		LOG_NOTICE( "WIF", "Config stored. Network: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString());		
		*/
		LOG_NOTICE( "WIF", "Blynk: key=" << sett.key << ", hostname=" << sett.hostname);
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
		/*IPAddress ip(sett.ip);
		IPAddress subnet(sett.subnet);
		IPAddress gw(sett.gw);
		LOG_NOTICE( "WIF", "Config loaded: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString());*/
		LOG_NOTICE( "WIF", "key=" << sett.key << " email=" << sett.email  << ", hostname=" << sett.hostname);
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
		sett.prev_value0 = 0.0;
		sett.prev_value1 = 0.0;

		/*IPAddress ip(192, 168, 1, 178);
		sett.ip = ip;
		
		IPAddress subnet(255, 255, 255, 0);
		sett.subnet = subnet;

		IPAddress gw(192, 168, 1, 1);
		sett.gw = gw;*/
		
		String key = "";
		strncpy(sett.key, key.c_str(), KEY_LEN);

		String hostname = BLYNK_DEFAULT_DOMAIN;
		strncpy(sett.hostname, hostname.c_str(), HOSTNAME_LEN);

		String email = "";
		strncpy(sett.email, email.c_str(), EMAIL_LEN);

		String email_title = "Новые показания {DEVICE_NAME}";
		strncpy(sett.email_title, email_title.c_str(), EMAIL_TITLE_LEN);

		String email_template = "ГВС: {V0} м3, ХВС: {V1} м3\\r\\nдельта:\\r\\nгвс: +{V3}, хвс: +{V4}\\r\\nпитание:{V2} В\\r\\nCMC:\\r\\nвода добавить: {V0} {V1}";
		strncpy(sett.email_template, email_template.c_str(), EMAIL_TEMPLATE_LEN);

		//LOG_NOTICE("WIF", "Init config. Network: IP=" << ip.toString() << ", Subnet=" << subnet.toString() << ", Gw=" << gw.toString());
		LOG_NOTICE("WIF", "key=" << key << " email=" << email  << ", hostname=" << hostname);
		return false;
	}
}