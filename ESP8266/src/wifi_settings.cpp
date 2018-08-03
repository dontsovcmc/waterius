
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
	{
		LOG_ERROR("WIF", "Config stored FAILED");
	}
	else
	{
		LOG_NOTICE("WIF", "Config stored OK");
	}
}


/* At every boot the IP information is loaded from EEPROM */
bool loadConfig(struct Settings &sett) 
{
	EEPROM.begin( 512 );

	EEPROM.get(0, sett);

	if (sett.crc == FAKE_CRC) 
	{
		//для безопасной работы с буферами, т.к. нигде в библиотеках нет проверок
		sett.key[KEY_LEN-1] = '\0';
		sett.hostname[HOSTNAME_LEN-1] = '\0';
		sett.email[EMAIL_LEN-1] = '\0';
		sett.email_title[EMAIL_TITLE_LEN-1] = '\0';
		sett.email_template[EMAIL_TEMPLATE_LEN-1] = '\0'; 
		
		LOG_NOTICE( "WIF", "key=" << sett.key << " email=" << sett.email  << ", hostname=" << sett.hostname);


		LOG_NOTICE( "WIF", "value0_start=" << sett.value0_start << ", impules0_start=" << sett.impules0_start << ", factor=" << sett.liters_per_impuls );
		LOG_NOTICE( "WIF", "value1_start=" << sett.value1_start << ", impules1_start=" << sett.impules1_start);
		
		return true;
	}
	else 
	{
		LOG_NOTICE( "WIF", "crc failed=" << sett.crc );

		memset(&sett, 0, sizeof(sett));

		sett.version = CURRENT_VERSION;  //для совместимости в будущем
		sett.liters_per_impuls = 10;
		
		String hostname = BLYNK_DEFAULT_DOMAIN;
		strncpy(sett.hostname, hostname.c_str(), HOSTNAME_LEN-1);

		String email_title = "New values {DEVICE_NAME}";
		strncpy(sett.email_title, email_title.c_str(), EMAIL_TITLE_LEN-1);

		String email_template = "Hot: {V0} m3, Cold: {V1} m3<br>day:<br>hot: +{V3}, cold: +{V4}<br>power:{V2}";
		strncpy(sett.email_template, email_template.c_str(), EMAIL_TEMPLATE_LEN-1);

		LOG_NOTICE("WIF", "version=" << sett.version << ", hostname=" << hostname);
		return false;
	}
}