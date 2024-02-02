from enum import Enum
from typing import List
from copy import deepcopy
import ipaddress


AS_COLD_CHANNEL = 7
AUTO_IMPULSE_FACTOR = 3


class CounterName(Enum):
    WATER_COLD = 0
    WATER_HOT = 1
    ELECTRO = 2
    GAS = 3
    HEAT = 4
    PORTABLE_WATER = 5
    OTHER = 6


class CounterType(Enum):
    NAMUR = 0
    DISCRETE = 1
    ELECTRONIC = 2
    HALL = 3
    NONE = 0xFF


WL_NO_SHIELD        = 255
WL_IDLE_STATUS      = 0
WL_NO_SSID_AVAIL    = 1
WL_SCAN_COMPLETED   = 2
WL_CONNECTED        = 3
WL_CONNECT_FAILED   = 4
WL_CONNECTION_LOST  = 5
WL_WRONG_PASSWORD   = 6
WL_DISCONNECTED     = 7

def check_ip_address(name, value):
    try:
        ipaddress.ip_address(value)
    except Exception as err:
        return {"errors": {name: "Некорректный адрес"}}
    return {}


def get_counter_img(i, name, ctype):
    if ctype == CounterType.HALL.value:
        if i == 0:
            return "meter-hall-0.png"
        if i == 1:
            return "meter-hall-1.png"
    elif ctype == CounterType.ELECTRONIC.value:
        if i == 0:
            return "meter-electro-0.png"
        if i == 1:
            return "meter-electro-1.png"
    elif name == CounterName.WATER_HOT.value:
        if i == 0:
            return "meter-hot-0.png"
        if i == 1:
            return "meter-hot-1.png"
    elif name == CounterName.GAS.value:
        if i == 0:
            return "meter-gas-0.png"
        if i == 1:
            return "meter-gas-1.png"
    if i == 0:
        return "meter-cold-0.png"
    if i == 1:
        return "meter-cold-1.png"
    return ""


attiny_link_error = False  # имитировать ошибку связи с МК


def variables_dictionary(obj) -> List[str]:
    return [attr for attr in dir(obj) if
                 not callable(getattr(obj, attr)) and
                 not attr.startswith("__")]


class InputSettings:
    input: int = 0  # служебный input_setup.html

    channel_start: float = 0.0
    serial: str = "000000"
    impulses_start: int = 0
    impulses: int = 0
    factor: int = 0

    # attiny
    counter_type: int = 0

    counter_name: int = 0   # преобразовать enum в int

    @property
    def counter_img(self):
        return get_counter_img(self.input, self.counter_name, self.counter_type)

    def __init__(self, **kwargs):
        self.apply_settings(dict(kwargs))

    def apply_settings(self, form_data):
        res = {}

        for k, v in form_data.items():
            if getattr(self, k) is not None:
                setattr(self, k, v)

        return res


input0_settings = InputSettings(
    input=0,
    counter_name=CounterName.WATER_HOT,
    factor=AS_COLD_CHANNEL,
    counter_type=CounterType.DISCRETE
)
input0_settings_vars = variables_dictionary(input0_settings)


input1_settings = InputSettings(
    input=1,
    counter_name=CounterName.WATER_COLD,
    factor=AUTO_IMPULSE_FACTOR,
    counter_type=CounterType.DISCRETE
)
input1_settings_vars = variables_dictionary(input1_settings)


class Settings:
    """
    Класс настроек в ESP
    Настройки ssid, password
    """
    waterius_on: bool | None = True
    waterius_host: str | None = "https://cloud.waterius.ru"
    waterius_key: str | None = "0102010200210512512052105125"
    waterius_email: str | None = "contact@waterius.ru"

    http_on: bool | None = False
    http_url: str | None = ""

    blynk_on: bool | None = False
    blynk_key: str | None = "182191205125"
    blynk_host: str | None = "blynk.com"

    mqtt_on: bool | None = False
    mqtt_host: str | None = ""
    mqtt_port: int | None = 1883
    mqtt_login: str | None = ""
    mqtt_password: str | None = ""
    mqtt_topic: str | None = "waterius/2713320"

    dhcp_off: bool | None = False
    ip: str | None = "192.168.0.100"
    gateway: str | None = "192.168.0.1"
    mask: str | None = "255.255.255.0"
    wifi_channel: int | None = 1
    mac_address: str | None = "00-1B-63-84-45-Е6"

    wakeup_per_min: int | None = 1440

    mqtt_auto_discovery: int | None = True
    mqtt_discovery_topic: str | None = "homeassistant"

    ntp_server: str | None = "ru.pool.ntp.org"

    ssid: str | None = "adsg"
    password: str | None = "123"

    wifi_phy_mode: int | None = 0

    attiny_version: int = 0


    @property
    def counter0_name(self):
        return input0_settings.counter_name

    @property
    def counter1_name(self):
        return input1_settings.counter_name

    @property
    def counter0_type(self):
        return input0_settings.counter_type

    @property
    def counter1_type(self):
        return input1_settings.counter_type

    @property
    def wifi_connect_status(self):
        if system_info.wifi_connect_status in [WL_NO_SSID_AVAIL, WL_CONNECT_FAILED, WL_CONNECTION_LOST]:
            return '8'  # S_WIFI_CONNECTION_LOST "Ошибка подключения. Попробуйте ещё раз.<br>Если не помогло, то пропишите статический ip. Еще можно зарезервировать MAC адрес Ватериуса в роутере. Если ничего не помогло, пришлите нам <a class='link' href='http://192.168.4.1/ssid.txt'>файл</a> параметров wi-fi сетей.";
        elif system_info.wifi_connect_status == WL_WRONG_PASSWORD:
            return '9'  # S_WL_WRONG_PASSWORD "Ошибка подключения: Некорректный пароль";
        elif system_info.wifi_connect_status == WL_IDLE_STATUS:
            return '10'  # S_WL_IDLE_STATUS "Ошибка подключения: Код 0";
        elif system_info.wifi_connect_status == WL_DISCONNECTED:
            return '11'  # S_WL_DISCONNECTED "Ошибка подключения: Отключен";
        elif system_info.wifi_connect_status == WL_NO_SHIELD:
            return '12'  # S_WL_NO_SHIELD "Ошибка подключения: Код 255";
        elif system_info.wifi_connect_status == WL_SCAN_COMPLETED:
            return '13'  # S_WL_SCAN_COMPLETED "Ошибка подключения: Код 2";
        elif system_info.wifi_connect_status == WL_CONNECTED:
            pass
        return ''

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
        res = {}
        #deepcopy(form_data)

        if 'waterius_on' in form_data:
            if not form_data.get('waterius_on') \
                    and not form_data.get('http_on') \
                    and not form_data.get('mqtt_on'):
                res.update({"errors": {
                    "form": "Выберите хотя бы одного получателя данных"
                }})

        # Сначала bool применим
        for k, v in form_data.items():
            if hasattr(self, k) and isinstance(getattr(self, k), bool):
                setattr(self, k, v)

        for k, v in form_data.items():
            if self.waterius_on:
                if k in ['waterius_host', 'waterius_email']:
                    continue

            if self.blynk_on:
                if k in ['blynk_key', 'blynk_host']:
                    continue

            if self.http_on:
                if k in ['http_url', ]:
                    continue

            if self.mqtt_on:
                if k in ['mqtt_host', 'mqtt_port',
                         'mqtt_login', 'mqtt_password',
                         'mqtt_topic']:
                    continue

                if self.mqtt_auto_discovery:
                    if k in ['mqtt_discovery_topic']:
                        continue

            if self.dhcp_off:
                if k in ['ip', 'gateway', 'mask']:
                    continue

            if k in settings_vars:
                err = {}
                if k in ['ip', 'gateway', 'mask']:
                    err = check_ip_address(k, v)

                if err:
                    res.update(err)
                else:
                    if hasattr(self, k):
                        setattr(self, k, v)

            if k == 'ssid':
                if v == 'ERROR_PASSWORD':
                    system_info.wifi_connect_status = WL_WRONG_PASSWORD
                elif v == 'ERROR_CONNECT':
                    system_info.wifi_connect_status = WL_CONNECT_FAILED
                else:
                    system_info.wifi_connect_status = WL_CONNECTED
        return res


class SystemInfo:
    version_esp: str = '0.11.9'
    version: int = 31
    fs_size = 256000
    fs_free = 50000
    build_date_time = '27 Nov 2023 12:00:00'
    wifi_connect_status: int = WL_IDLE_STATUS


settings = Settings()
settings_vars = variables_dictionary(settings)


system_info = SystemInfo()
system_info_vars = variables_dictionary(system_info)
