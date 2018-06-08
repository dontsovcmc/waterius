#include "WifiSettings.h"
#include "MasterI2C.h"
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>
#include "SetupAP.h"
#include "SenderBlynk.h"
#include "SenderTCP.h"

/*
TODO: crc настроек

*/

MasterI2C masterI2C;

SlaveData data;
Settings sett;

void setup() {
	memset(&data, 0, sizeof(data));

	LOG_BEGIN(115200);
	while (!Serial) 
		;

	LOG_NOTICE( "ESP", "Booted" );

	ESP.wdtDisable();
	
	masterI2C.begin();
}

void CalculateLitres(Settings &sett, SlaveData &data) {

	LOG_NOTICE( "ESP", "new impulses=" << data.value0 << " " << data.value1);
	sett.value0 = sett.litres0_start + (data.value0 - sett.impules0_start)*sett.liters_per_impuls/1000.0;
	sett.value1 = sett.litres1_start + (data.value1 - sett.impules1_start)*sett.liters_per_impuls/1000.0;
	LOG_NOTICE( "ESP", "new values=" << sett.value0 << " " << sett.value1);
	storeConfig(sett);
}

void loop() {

	data.diagnostic = masterI2C.getSlaveData(data); //нужны и в настройке

	if (masterI2C.setup_mode()) {
		loadConfig(sett);
		setup_ap(sett, data);
	}
	else {
		if (loadConfig(sett)) {
			
			CalculateLitres(sett, data);

			if (SenderBlynk::send(sett, data)) {
            	LOG_NOTICE("BLK", "send ok");
			}

			//if (SenderTCP::send(sett, data)) {
            //	LOG_NOTICE("TCP", "send ok");
			//}
			else {
				LOG_ERROR("ESP", "error sending data");
			}
			
		} else {
			LOG_ERROR("ESP", "error loading config");
		}
	}

	LOG_NOTICE( "ESP", "Going to sleep" );
	masterI2C.sendCmd( 'Z' );       // "Можешь идти спать"
	ESP.deepSleep( 0, RF_DEFAULT );	// "Спим до следущего включения EN"
}
