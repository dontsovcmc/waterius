from dataclasses import dataclass
from enum import Enum
from copy import deepcopy
import ipaddress

AUTO_IMPULSE_FACTOR = 3
AS_COLD_CHANNEL = 7


class CounterName(Enum):
    WATER_COLD = 0
    WATER_HOT = 1
    ELECTRO = 2
    GAS = 3
    HEAT = 4
    PORTABLE_WATER = 5
    OTHER = 6


def check_ip_address(name, value):
    try:
        ipaddress.ip_address(value)
    except Exception as err:
        return {"errors": {name: "Некорректный адрес"}}
    return {}


class Settings:
    """
    Класс настроек в ESP
    Настройки ssid, password
    """

    waterius_host: str | None = "https://cloud.waterius.ru"
    waterius_key: str | None = "0102010200210512512052105125"
    waterius_email: str | None = "contact@waterius.ru"

    blynk_key: str | None = "182191205125"
    blynk_host: str | None = "blynk.com"

    mqtt_host: str | None = ""
    mqtt_port: int | None = 1883
    mqtt_login: str | None = ""
    mqtt_password: str | None = ""
    mqtt_topic: str | None = ""

    channel0_start: float | None = 0.0
    channel1_start: float | None = 0.0

    serial0: str | None = "00-hot"
    serial1: str | None = "10-cold"

    impulses0_start: int | None = 0
    impulses1_start: int | None = 0

    dhcp_on: bool | None = True
    ip: str | None = "192.168.0.100"
    gateway: str | None = "192.168.0.1"
    mask: str | None = "255.255.255.0"
    mac_address: str | None = "00-1B-63-84-45-Е6"

    wakeup_per_min: int | None = 1440,

    mqtt_auto_discovery: int | None = 0
    mqtt_discovery_topic: str | None = ""

    ntp_server: str | None = "ru.pool.ntp.org"

    ssid: str | None = "adsg"
    password: str | None = "123"

    wifi_phy_mode: int | None = 0

    counter0_name: int | None = CounterName.WATER_HOT
    counter1_name: int | None = CounterName.WATER_COLD

    factor0: int | None = AS_COLD_CHANNEL
    factor1: int | None = AUTO_IMPULSE_FACTOR

    def apply_settings(self, form_data):
        """
        Удаляем поля где значение null
        Проверяем настройки на корректность
        :param form_data: dict
        :return:
        {...form_data...} - успех

        Если есть ошибки:
        {...form_data...
            "errors": {
                "serial1": "ошибка"
            }
        }
        """
        res = deepcopy(form_data)

        static_ip_on = form_data.get('static_ip_on')

        for k, v in form_data.items():

            if k in ['ip', 'gateway', 'mask']:
                if static_ip_on:
                    res.update(check_ip_address(k, v))

            if k in settings_vars:
                setattr(self, k, v)

        return res


settings = Settings()
settings_vars = [attr for attr in dir(settings) if
                 not callable(getattr(settings, attr)) and
                 not attr.startswith("__")]


class AttinyData:
    impulses0: int = 0
    impulses1: int = 0
    counter_type0: int = 0
    counter_type1: int = 0
    model: int = 30


attiny_data = AttinyData()

attiny_link_error = False  # имитировать ошибку связи с МК
