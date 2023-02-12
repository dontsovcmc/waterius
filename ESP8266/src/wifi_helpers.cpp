#include "wifi_helpers.h"
#include "Logging.h"
#include "utils.h"

#define WIFI_SET_MODE_ATTEMPTS 2

// Adoption of Tasmota wifi module
// https://github.com/arendst/Tasmota/blob/development/tasmota/tasmota_support/support_wifi.ino

void wifi_set_mode(WiFiMode_t wifi_mode)
{
  if (WiFi.getMode() == wifi_mode)
  {
    return;
  }

  if (wifi_mode != WIFI_OFF)
  {
    WiFi.forceSleepWake();
    delay(100);
  }

  uint32_t attempts = WIFI_SET_MODE_ATTEMPTS;
  while (!WiFi.mode(wifi_mode) && attempts--)
  {
    LOG_INFO(F("WIFI: Retry set Mode..."));
    delay(100);
  }

  if (wifi_mode == WIFI_OFF)
  {
    delay(1000); // TODO: наверное нужно уменьшить до 100-300 мсек
    WiFi.forceSleepBegin();
    delay(1);
  }
  else
  {
    delay(30); // Must allow for some time to init.
  }
}

void wifi_begin(Settings &sett)
{

  WiFi.persistent(false);
  WiFi.disconnect();

  delay(200); // подождем чтобы проинициализировалась сеть

  wifi_set_mode(WIFI_STA);

  if (!WiFi.getAutoConnect())
  {
    WiFi.setAutoConnect(true);
  }

  if (is_dhcp(sett))
  {
    WiFi.config(sett.ip, sett.gateway, sett.mask, sett.gateway, IPAddress().fromString(DEF_FALLBACK_DNS));
  }

  WiFi.hostname(get_device_name());

  delay(100); // подождем чтобы проинициализировалась сеть

  if (sett.wifi_channel)
  {
    WiFi.begin(sett.wifi_ssid, sett.wifi_password, sett.wifi_channel, sett.wifi_bssid);
  }
  else
  {
    WiFi.begin(sett.wifi_ssid, sett.wifi_password);
  }

  WiFi.waitForConnectResult(ESP_CONNECT_TIMEOUT); //  ждем результата подключения
}

void wifi_shutdown()
{
  WiFi.disconnect(true);
  delay(100);
  wifi_set_mode(WIFI_OFF);
}

String wifi_mode()
{
  // WiFi.setPhyMode(WIFI_PHY_MODE_11B = 1, WIFI_PHY_MODE_11G = 2, WIFI_PHY_MODE_11N = 3);
  WiFiPhyMode_t m = WiFi.getPhyMode();
  String mode;
  switch (m)
  {
  case WIFI_PHY_MODE_11B:
    mode = F("B");
    break;
  case WIFI_PHY_MODE_11G:
    mode = F("G");
    break;
  case WIFI_PHY_MODE_11N:
    mode = F("N");
    break;
  default:
    mode = (int)m;
    break;
  }
  return mode;
}

bool wifi_connect(Settings &sett)
{
  uint32_t start_time = millis();
  LOG_INFO(F("WIFI: Connecting..."));
  int attempts = WIFI_CONNECT_ATTEMPTS;
  do
  {
    wifi_begin(sett);
    if (WiFi.isConnected())
    {
      sett.wifi_channel = WiFi.channel(); // сохраняем для быстрого коннекта
      uint8_t *bssid = WiFi.BSSID();
      memcpy((void *)&sett.wifi_bssid, (void *)bssid, sizeof(sett.wifi_bssid)); // сохраняем для быстрого коннекта
      LOG_INFO(F("WIFI: Connected."));
      LOG_INFO(F("WIFI: SSID: ") << WiFi.SSID() << F(" Channel: ") << WiFi.channel() << F(" BSSID: ") <<  WiFi.BSSIDstr());
      LOG_INFO(F("WIFI: Time spent ") << millis() - start_time << F(" ms"));
      return true;
    }
    sett.wifi_channel = 0;
    LOG_ERROR(F("WIFI: Connection failed."));
    LOG_ERROR(F("WIFI: Tries #") << WIFI_CONNECT_ATTEMPTS - attempts + 1 << F(" from ") << WIFI_CONNECT_ATTEMPTS);
  } while (attempts--);
  
  LOG_ERROR(F("WIFI: Connection failed.") << millis() - start_time << F(" ms"));
  return false;
}