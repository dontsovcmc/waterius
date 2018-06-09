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

	LOG_NOTICE( "ESP", "Booted" );

	ESP.wdtDisable();
	
	masterI2C.begin();
}

void calculate_values(Settings &sett, float *value0, float *value1) {

	LOG_NOTICE( "ESP", "new impulses=" << sett.impules0 << " " << sett.impules1);

	*value0 = sett.value0_start + (sett.impules0 - sett.impules0_start)/1000.0*sett.liters_per_impuls;
	*value1 = sett.value1_start + (sett.impules1 - sett.impules1_start)/1000.0*sett.liters_per_impuls;

	LOG_NOTICE( "ESP", "new values=" << *value0 << " " << *value1);
}

void loop() {

	data.diagnostic = masterI2C.getSlaveData(data); //нужны и в настройке

	if (masterI2C.setup_mode()) {
		loadConfig(sett);
		setup_ap(sett, data);
	}
	else {
		if (loadConfig(sett)) {
			
			sett.impules0 = data.impulses0;
			sett.impules1 = data.impulses1; 
			storeConfig(sett);

			float value0, value1;
			calculate_values(sett, &value0, &value1);

			if (send_blynk(sett, value0, value1, data.voltage / 1000.0)) {
            	LOG_NOTICE("BLK", "send ok");
			}

			//if (send_tcp(sett, data)) {
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
