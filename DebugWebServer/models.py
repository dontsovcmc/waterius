
from dataclasses import dataclass
from fastapi import Form


@dataclass
class ConnectModel:
    ssid: str = Form(...)
    password: str | None = Form(None)

    dhcp_on: bool | None = Form(None)
    gateway_ip: str | None = Form(None)
    static_ip: str | None = Form(None)
    mask: str | None = Form(None)
    mac_address: str | None = Form(None)
    wifi_channel: int | None = Form(None)
    wifi_phy_mode: int | None = Form(None)

    wizard: bool | None = Form(None)

@dataclass
class SettingsModel:
    """
    Класс настроек в ESP
    POST запрос от фронтенда на сохранение их
    """

    # Страница /setup_send.html
    waterius_on: bool | None = Form(None)
    waterius_email: str | None = Form(None)

    blynk_on: bool | None = Form(None)
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

    mqtt_auto_discovery: int | None = Form(None)
    mqtt_discovery_topic: str | None = Form(None)

    ntp_server: str | None = Form(None)

    wifi_phy_mode: int | None = Form(None)

    # for save
    input: int | None = Form(None)  # input_setup.html
    counter_name: int | None = Form(None)
    factor: int | None = Form(None)
    counter_type: int | None = Form(None)
    channel_start: float | None = Form(None)
    serial: str | None = Form(None)


@dataclass
class InputModel:
    input: int | None = Form(None)  # input_setup.html
    counter_name: int | None = Form(None)
    factor: int | None = Form(None)
    counter_type: int | None = Form(None)
    channel_start: float | None = Form(None)
    serial: str | None = Form(None)
