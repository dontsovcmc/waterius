
from dataclasses import dataclass
from fastapi import Form
from esp import settings

@dataclass
class ConnectModel:
    ssid: str = Form(...)
    password: str = Form(...)

    dhcp_on: bool = Form(...)
    gateway_ip: str = Form(...)
    static_ip: str = Form(...)
    mask: str = Form(...)
    mac_address: str = Form(...)


@dataclass
class SettingsModel:
    """
    Класс настроек в ESP
    Настройки ssid, password
    """

    # Страница /setup_send.html
    waterius_on: bool | None = Form(None)
    waterius_email: str | None = Form(None)

    blink_on: bool | None = Form(None)
    blynk_key: str | None = Form(None)
    blynk_host: str | None = Form(None)

    http_on: bool | None = Form(None)
    http_url: str | None = Form(None)

    mqtt_on: bool | None = Form(None)
    mqtt_host: str | None = Form(None)
    mqtt_port: int | None = Form(None)
    mqtt_login: str | None = Form(None)
    mqtt_password: str | None = Form(None)
    mqtt_topic: str | None = Form(None)

    wakeup_per_min: int | None = Form(None)

    #
    channel0_start: float | None = Form(None)
    serial0: str | None = Form(None)

    channel1_start: float | None = Form(None)
    serial1: str | None = Form(None)

    mqtt_auto_discovery: int | None = Form(None)
    mqtt_discovery_topic: str | None = Form(None)

    ntp_server: str | None = Form(None)

    wifi_phy_mode: int | None = Form(None)

    counter0_name: int | None = Form(None)
    counter1_name: int | None = Form(None)

    factor0: int | None = Form(None)
    factor1: int | None = Form(None)
