from enum import Enum
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


def check_ip_address(name, value):
    try:
        ipaddress.ip_address(value)
    except Exception as err:
        return {"errors": {name: "Некорректный адрес"}}
    return {}


def get_counter_title(name):

    titles = {
        CounterName.WATER_COLD: "Холодная вода",
        CounterName.WATER_HOT: "Горячая вода",
        CounterName.ELECTRO: "Электричество",
        CounterName.GAS: "Газ",
        CounterName.HEAT: "Тепло",
        CounterName.PORTABLE_WATER: "Питьевая вода",
        CounterName.OTHER: "Другой"
    }
    if CounterName(name) in titles:
        return titles[CounterName(name)]
    return "Другой"


def get_counter_img(name):
    img = {
        CounterName.WATER_COLD: "meter-cold.png",
        CounterName.WATER_HOT: "meter-hot.png",
    }
    if CounterName(name) in img:
        return img[CounterName(name)]
    return ""


def get_counter_instruction(name):

    instr = {
        CounterName.WATER_COLD: "Спускайте воду в унитазе пока устройство не перенесёт вас на следующую страницу",
        CounterName.WATER_HOT: "Откройте кран горячей воды пока устройство не перенесёт вас на следующую страницу",
        CounterName.ELECTRO: "Включите электроприбор. После моргания светодиода должна открыться следующая страница. "
                             "Если не открывается, значит некорректное подключение или счётчик не поддерживается.",
        CounterName.GAS: "Приход импульса от газового счётчика долго ожидать, нажмите Пропустить и продолжите настройку.",
        CounterName.HEAT: "Приход импульса от счётчика тепла долго ожидать, нажмите Пропустить и продолжите настройку.",
        CounterName.PORTABLE_WATER: "Откройте кран питьевой воды пока устройство не перенесёт вас на следующую страницу",
    }

    if CounterName(name) in instr:
        return instr[CounterName(name)]
    return "При приходе импульса от счётчика устройство перенесёт вас на следующую страницу"


class AttinyData:
    impulses0: int = 0
    impulses1: int = 0
    counter_type0: int = 0
    counter_type1: int = 0
    model: int = 30


attiny_data = AttinyData()
attiny_link_error = False  # имитировать ошибку связи с МК


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

    channel0_start: float | None = 0.0
    channel1_start: float | None = 0.0

    serial0: str | None = "00-hot"
    serial1: str | None = "10-cold"

    impulses0_start: int | None = 0
    impulses1_start: int | None = 0

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

    counter0_name: int | None = 1  # CounterName.WATER_HOT
    counter1_name: int | None = 0  # CounterName.WATER_COLD

    factor0: int | None = AS_COLD_CHANNEL
    factor1: int | None = AUTO_IMPULSE_FACTOR


    @property
    def counter0_type(self):
        return attiny_data.counter_type0

    @counter0_type.setter
    def counter0_type(self, value):
        attiny_data.counter_type0 = value

    @property
    def counter1_type(self):
        return attiny_data.counter_type1

    @counter1_type.setter
    def counter1_type(self, value):
        attiny_data.counter_type1 = value

    @property
    def counter0_title(self):
        return get_counter_title(self.counter0_name)

    @property
    def counter1_title(self):
        return get_counter_title(self.counter1_name)

    @property
    def counter0_instruction(self):
        return get_counter_instruction(self.counter0_name)

    @property
    def counter1_instruction(self):
        return get_counter_instruction(self.counter1_name)

    @property
    def counter1_img(self):
        return get_counter_img(self.counter1_name)

    @property
    def counter0_img(self):
        return get_counter_img(self.counter0_name)

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
            if isinstance(getattr(self, k), bool):
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
                    setattr(self, k, v)

        return res


class SystemInfo:
    version_esp: str = '0.11.9'
    version: int = 31
    fs_size = 256000
    fs_free = 50000
    build_date_time = '27 Nov 2023 12:00:00'

    @property
    def wifi_connect_status(self):
        # WL_NO_SSID_AVAIL
        # WL_CONNECT_FAILED
        # WL_CONNECTION_LOST
        #return "Ошибка подключения. Попробуйте ещё раз.<br>Если не помогло, то пропишите статический ip. Еще можно зарезервировать MAC адрес Ватериуса в роутере. Если ничего не помогло, пришлите нам <a class=\"link\" href=\"http://192.168.4.1/ssid.txt\">файл</a> параметров wi-fi сетей.'
        # WL_WRONG_PASSWORD
        #return 'Ошибка подключения: Некорректный пароль'
        # WL_NO_SHIELD
        # WL_IDLE_STATUS
        # WL_SCAN_COMPLETED
        # WL_CONNECTED
        # WL_DISCONNECTED
        return ''


settings = Settings()
settings_vars = [attr for attr in dir(settings) if
                 not callable(getattr(settings, attr)) and
                 not attr.startswith("__")]

system_info = SystemInfo()
system_info_vars = [attr for attr in dir(system_info) if
                    not callable(getattr(system_info, attr)) and
                    not attr.startswith("__")]
