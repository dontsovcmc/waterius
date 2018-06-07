#include "MyWifi.h"
#include "MasterI2C.h"
#include "Logging.h"
#include <user_interface.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

MasterI2C masterI2C;
MyWifi myWifi;

SlaveData data;
Settings sett;

void setup() {
	LOG_BEGIN(115200);
	while (!Serial) 
		;

	LOG_NOTICE( "ESP", "Booted" );

	ESP.wdtDisable();
	
	masterI2C.begin();
}

void loop() {

	if (masterI2C.setup_mode()) {
		loadConfig(sett);
		myWifi.setup(sett);
	}
	else {
		if (loadConfig(sett)) {
			

			Blynk.begin(sett.key, WiFi.SSID().c_str(), WiFi.psk().c_str());
			LOG_NOTICE( "ESP", "Blynk beginned");

			Blynk.run();
			LOG_NOTICE( "ESP", "Blynk run");

			Blynk.virtualWrite(V0, data.value1);
			Blynk.virtualWrite(V1, data.value2);
			Blynk.virtualWrite(V2, data.voltage);

			LOG_NOTICE("ESP", "send ok");

			/*
			if (myWifi.connect(sett)) {
				LOG_NOTICE( "ESP", "Wifi-connected" );

				data.diagnostic = masterI2C.getSlaveData(data);

				if (!myWifi.send(sett, (void*)&data, sizeof(SlaveData))) {
					LOG_ERROR( "ESP", "send data failed" );
				}
				
				if (!data.diagnostic) {
					LOG_ERROR( "ESP", "getSlaveData failed" );
				}

			} else {
				LOG_ERROR( "ESP", "Wifi connected false, go sleep" );
			}*/

		} else {
			LOG_ERROR("ESP", "error loading config");
		}
	}

	LOG_NOTICE( "ESP", "Going to sleep" );
	masterI2C.sendCmd( 'Z' );       // "Можешь идти спать"
	ESP.deepSleep( 0, RF_DEFAULT );	// "Спим до следущего включения EN"
}
