from enum import Enum

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


class Settings:
    """
    Класс настроек в ESP
    Настройки ssid, password
    """
    dhcp_on: bool | None = False

    waterius_host: str = "https://cloud.waterius.ru"
    waterius_key: str = "0102010200210512512052105125"
    waterius_email: str = "contact@waterius.ru"

    blynk_key: str = "182191205125"
    blynk_host: str = "blynk.com"

    mqtt_host: str = ""
    mqtt_port: int = 1883
    mqtt_login: str = ""
    mqtt_password: str = ""
    mqtt_topic: str = ""

    channel0_start: float = 0.0
    channel1_start: float = 0.0

    serial0: str = ""
    serial1: str = ""

    impulses0_start: int = 0
    impulses1_start: int = 0

    static_ip_on: bool | None = False
    ip: str = ""
    gateway: str = "192.168.0.1"
    mask: str = "255.255.255.0"
    mac_address: str = "00-1B-63-84-45-Е6"

    wakeup_per_min: int = 1440,

    mqtt_auto_discovery: int = 0
    mqtt_discovery_topic: str = ""

    ntp_server: str = "ru.pool.ntp.org"

    ssid: str = ""
    password: str = ""

    wifi_phy_mode: int = 0

    counter0_name: int = CounterName.WATER_HOT
    counter1_name: int = CounterName.WATER_COLD

    factor0: int = AS_COLD_CHANNEL
    factor1: int = AUTO_IMPULSE_FACTOR


settings = Settings()


class AttinyData:
    impulses0: int = 0
    impulses1: int = 0
    counter_type0: int = 0
    counter_type1: int = 0
    model: int = 30


attiny_data = AttinyData()

attiny_link_error = False  # имитировать ошибку связи с МК
