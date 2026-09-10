"""
Настройки стенда в одном месте: адреса, пины и порты, которые меняются от
стенда к стенду. Файл `stand.ini` не коммитится, рядом лежит stand.ini.example.
"""

from __future__ import annotations

import configparser
import os
from dataclasses import dataclass
from pathlib import Path

DEFAULT_PATH = Path(__file__).with_name('stand.ini')


@dataclass(frozen=True)
class StandConfig:
    # Плата-манипулятор
    metf_host: str
    # Разрешено ли стенду выдавать на вход уровень, а не только замыкание.
    # Нужно для типа «Электронный (+)»; провод тот же, но плата начинает
    # питать линию, поэтому включается отдельно и осознанно.
    can_drive_high: bool
    button_pin: int
    ch0_pin: int
    ch1_pin: int
    reset_pin: int

    # Лабораторная точка доступа
    router_port: str
    router_host: str
    router_password: str
    ap_ssid: str          # пусто - спросим у самой точки доступа
    ap_password: str      # у роутера не прочитать: show config печатает звёздочки

    # Ватериус в сети точки доступа
    dut_mac: str
    dut_ip: str

    # Приёмник посылок: адрес, который прошит в настройках Ватериуса
    receiver_host: str
    receiver_port: int
    receiver_tls_port: int    # тот же приёмник по https, для проверки G8

    # AT-плата: HTTP-клиент в сети портала Ватериуса
    atboard_port: str

    # Брокер
    broker_host: str
    broker_port: int
    mqtt_topic: str

    @property
    def http_url(self) -> str:
        return f'http://{self.receiver_host}:{self.receiver_port}/data'

    @property
    def https_url(self) -> str:
        return f'https://{self.receiver_host}:{self.receiver_tls_port}/data'

    def https_file(self, path: str) -> str:
        """Адрес файла, который раздаёт приёмник по https: образы OTA."""
        return f'https://{self.receiver_host}:{self.receiver_tls_port}{path}'


def load(path: str | os.PathLike[str] | None = None) -> StandConfig:
    """
    Прочитать stand.ini. Любое значение перекрывается переменной окружения
    вида HIL_METF_HOST - удобно, когда стендов два.
    """
    parser = configparser.ConfigParser()
    parser.read(path or DEFAULT_PATH, encoding='utf-8')

    def get(section: str, key: str, default: str = '') -> str:
        env = os.environ.get(f'HIL_{section.upper()}_{key.upper()}')
        if env:
            return env
        return parser.get(section, key, fallback=default)

    return StandConfig(
        metf_host=get('metf', 'host', '192.168.51.250'),
        can_drive_high=get('metf', 'can_drive_high', '0') == '1',
        button_pin=int(get('metf', 'button_pin', '1')),
        ch0_pin=int(get('metf', 'ch0_pin', '3')),
        ch1_pin=int(get('metf', 'ch1_pin', '2')),
        reset_pin=int(get('metf', 'reset_pin', '0')),
        router_port=get('router', 'port', ''),
        router_host=get('router', 'host', ''),
        router_password=get('router', 'password', ''),
        ap_ssid=get('router', 'ap_ssid', ''),
        ap_password=get('router', 'ap_password', ''),
        dut_mac=get('dut', 'mac', ''),
        dut_ip=get('dut', 'ip', '192.168.4.100'),
        receiver_host=get('receiver', 'host', '192.168.4.2'),
        receiver_port=int(get('receiver', 'port', '8000')),
        receiver_tls_port=int(get('receiver', 'tls_port', '8443')),
        atboard_port=get('atboard', 'port', ''),
        broker_host=get('broker', 'host', '192.168.4.2'),
        broker_port=int(get('broker', 'port', '1883')),
        mqtt_topic=get('broker', 'topic', 'waterius'),
    )
