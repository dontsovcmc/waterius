
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>

#include "wifi_settings.h"
#include "master_i2c.h"
#include "setup_ap.h"
#include "sender_blynk.h"
#include "sender_tcp.h"


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

void calculate_values(Settings &sett, SlaveData &data, float *value0, float *value1) {

	LOG_NOTICE( "ESP", "new impulses=" << data.impulses0 << " " << data.impulses1);

	*value0 = sett.value0_start + (data.impulses0 - sett.impules0_start)/1000.0*sett.liters_per_impuls;
	*value1 = sett.value1_start + (data.impulses1 - sett.impules1_start)/1000.0*sett.liters_per_impuls;

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
			
			float value0, value1;
			calculate_values(sett, data, &value0, &value1);

#ifdef SEND_BLYNK
			if (send_blynk(sett, value0, value1, data.voltage / 1000.0)) {
            	LOG_NOTICE("BLK", "send ok");
			}
#endif
#ifdef SEND_TCP
			if (send_tcp(sett, value0, value1, data.voltage / 1000.0)) {
            	LOG_NOTICE("TCP", "send ok");
			}
#endif

		} else {
			LOG_ERROR("ESP", "error loading config");
		}
	}

	LOG_NOTICE( "ESP", "Going to sleep" );
	masterI2C.sendCmd( 'Z' );       // "Можешь идти спать"
	ESP.deepSleep( 0, RF_DEFAULT );	// "Спим до следущего включения EN"
}
