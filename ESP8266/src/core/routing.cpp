#include "routing.h"

bool is_waterius_site(const Settings &sett)
{
	return sett.waterius_on && sett.waterius_host[0] && sett.waterius_key[0];
}

bool is_http(const Settings &sett)
{
	return sett.http_on && sett.http_url[0];
}

bool is_mqtt(const Settings &sett)
{
	return sett.mqtt_on && sett.mqtt_host[0];
}

bool is_ha(const Settings &sett)
{
	return is_mqtt(sett) && sett.mqtt_auto_discovery;
}

bool is_dhcp(const Settings &sett)
{
	return sett.dhcp_off == 0;
}
