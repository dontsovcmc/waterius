"""
Настройки стенда в одном месте: адреса, пины и порты, которые меняются от
стенда к стенду. Файл `stand.ini` не коммитится, рядом лежит stand.ini.example.
"""

from __future__ import annotations

import configparser
import os
import socket
from dataclasses import dataclass
from pathlib import Path

from loguru import logger

DEFAULT_PATH = Path(__file__).with_name('stand.ini')

# Куда спрашивать дорогу, если в файле нет ни роутера, ни METF. Адрес из
# TEST-NET-3 (RFC 5737): маршрут до него уходит в шлюз по умолчанию, а пакетов
# не будет - connect() у UDP ничего не шлёт.
ELSEWHERE = '203.0.113.1'


def own_ip(towards: str) -> str:
    """
    Адрес этой машины, с которого ядро пошло бы к `towards`.

    Сокет UDP только соединяют: пакет не уходит, зато таблица маршрутов уже
    выбрана и getsockname отдаёт нужный интерфейс. Перебирать интерфейсы руками
    нельзя - на Маке поднято с десяток туннелей VPN (utun*) и link-local
    169.254.x, а стенду нужен адрес именно в проводной сети роутера.
    """
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect((towards, 9))          # discard: порт не важен
        return str(probe.getsockname()[0])
    finally:
        probe.close()


def _pin(path: Path, section: str, key: str, value: str) -> bool:
    """
    Переписать одну строку stand.ini, не тронув остальные.

    configparser.write() вернул бы файл без единого комментария, а половина
    стенда объяснена именно ими: какой канал держать, почему пуст порт роутера,
    зачем закреплён канал точки.
    """
    try:
        lines = path.read_text(encoding='utf-8').splitlines(keepends=True)
    except OSError:
        return False
    here = ''
    for i, line in enumerate(lines):
        bare = line.strip()
        if bare.startswith('[') and bare.endswith(']'):
            here = bare[1:-1].strip().lower()
            continue
        if here != section:
            continue
        name, sign, _ = line.partition('=')
        # Комментарий вида «; host = 10.0.0.1» под это не подходит: у него в
        # имени остаётся точка с запятой.
        if sign and name.strip().lower() == key:
            lines[i] = f'{name.rstrip()} = {value}\n'
            path.write_text(''.join(lines), encoding='utf-8')
            return True
    return False


@dataclass(frozen=True)
class StandConfig:
    # Плата-манипулятор
    metf_host: str
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
    # Канал точки стенда закреплён, а не оставлен на «как получилось»: эфир
    # решает судьбу прогона (05_air-and-loss.md). Запасной нужен тестам смены
    # канала - они уводят точку на него и возвращают обратно.
    ap_channel: int
    ap_channel_other: int
    # Полоса, которую мы считаем правильной. 20 МГц: ESP8266 шире не умеет, а
    # 40 МГц занимают пять каналов вместо одного и бьют по соседям.
    ap_bandwidth: int

    # Прогон без платы WT32-ETH01 (--norouter): точки стенда нет, Ватериус живёт
    # в обычной сети из [wifi], а тесты, управляющие точкой, пропускаются.
    norouter: bool
    # Эта самая сеть. Спросить её имя не у кого - точки нет, - поэтому только файл.
    wifi_ssid: str
    wifi_password: str

    # Прогон без cloud.waterius.ru (--nocloud): получатель «облако» у устройства
    # выключен, а тесты, которые сверяют его ответ, пропускаются.
    nocloud: bool

    # Ватериус в сети точки доступа
    dut_mac: str
    dut_ip: str
    # Почта учётной записи, к которой cloud.waterius.ru привязывает устройство:
    # с пустой облако отвечает 404 на каждую посылку (прогон 29.09).
    dut_email: str

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
    def dut_ssid(self) -> str:
        """
        Сеть, в которой обязан жить Ватериус: точка стенда либо [wifi].

        Имя точки стенда бывает пустым - тогда его спрашивают у самой точки
        (`Stand.ap_ssid`). Без роутера спрашивать не у кого, и пустое имя здесь
        уже беда, о которой говорит подъём.
        """
        return self.wifi_ssid if self.norouter else self.ap_ssid

    @property
    def dut_password(self) -> str:
        """Пароль той же сети: у роутера его не прочитать, только из файла."""
        return self.wifi_password if self.norouter else self.ap_password

    @property
    def http_url(self) -> str:
        return f'http://{self.receiver_host}:{self.receiver_port}/data'

    @property
    def https_url(self) -> str:
        return f'https://{self.receiver_host}:{self.receiver_tls_port}/data'

    def https_file(self, path: str) -> str:
        """Адрес файла, который раздаёт приёмник по https: образы OTA."""
        return f'https://{self.receiver_host}:{self.receiver_tls_port}{path}'


def load(path: str | os.PathLike[str] | None = None,
         search: bool = False, norouter: bool = False,
         nocloud: bool = False) -> StandConfig:
    """
    Прочитать stand.ini. Любое значение перекрывается переменной окружения
    вида HIL_METF_HOST - удобно, когда стендов два.

    `search` - искать ли платы, чей записанный адрес молчит (discover.py).
    Прогону и спасательным инструментам это нужно, разбору настроек - нет:
    поиск ходит по сети.

    `norouter` - прогон без платы WT32-ETH01. Её не ищут (обход /24 ради платы,
    которой нет, стоит пары секунд и одной строки лжи в stand.ini), а сеть
    устройства берётся из [wifi].
    """
    parser = configparser.ConfigParser()
    file = Path(path or DEFAULT_PATH)
    parser.read(file, encoding='utf-8')

    def get(section: str, key: str, default: str = '') -> str:
        env = os.environ.get(f'HIL_{section.upper()}_{key.upper()}')
        if env:
            return env
        return parser.get(section, key, fallback=default)

    # Дорогу спрашиваем к роутеру стенда, а не «в интернет»: приёмник и брокер
    # ждут Ватериус, который приходит к ним через NAT точки доступа, - значит
    # нужен адрес Мака в той же проводной сети, где стоят роутер и METF.
    towards = get('router', 'host', '') or get('metf', 'host', '') or ELSEWHERE
    try:
        mine = own_ip(towards)
    except OSError as err:
        logger.warning(f'свой адрес не спросить ({err}): адреса берутся из файла как есть')
        mine = ''

    def fix(section: str, host: str, why: str) -> str:
        """Принять найденный адрес и починить строку в файле."""
        written = parser.get(section, 'host', fallback='')
        if host != written:
            logger.warning(
                f'[{section}] host в stand.ini - {written or "пусто"}, а {why} {host}: '
                'беру найденный'
                + ('' if _pin(file, section, 'host', host) else ' (файл не поправлен)'))
        return host

    def here(section: str) -> str:
        """
        Адрес этой машины для приёмника и брокера.

        Оба слушают на Маке, а его адрес выдаёт DHCP: 28 сентября он сменился
        с .18 на .11, и весь прогон умер на первой фикстуре - «брокер не
        отвечает на 192.168.100.18:1883». Поэтому написанное в файле не берут
        на веру: адрес спрашивают у ядра перед каждым прогоном, а разошедшуюся
        строку stand.ini чинят на месте, чтобы следующий прогон не начинался с
        того же отказа. Переменная окружения главнее - ею приёмник уводят на
        другую машину.
        """
        env = os.environ.get(f'HIL_{section.upper()}_HOST')
        if env:
            return env
        if not mine:
            return parser.get(section, 'host', fallback='')
        return fix(section, mine, 'эта машина')

    def board(kind: str, section: str) -> str:
        """
        Адрес платы стенда. Молчащий адрес - повод поискать плату, а не повод
        падать по таймауту через тридцать секунд: адреса раздаёт DHCP, и они
        переезжают целыми стендами (discover.py).
        """
        written = parser.get(section, 'host', fallback='')
        env = os.environ.get(f'HIL_{section.upper()}_HOST')
        if env:
            return env
        if not search or not mine:
            return written
        if norouter and kind == 'router':
            return written
        from . import discover
        found = discover.find(kind, written, mine)
        return fix(section, found, 'плата нашлась на') if found else written

    return StandConfig(
        metf_host=board('metf', 'metf'),
        button_pin=int(get('metf', 'button_pin', '1')),
        ch0_pin=int(get('metf', 'ch0_pin', '3')),
        ch1_pin=int(get('metf', 'ch1_pin', '2')),
        reset_pin=int(get('metf', 'reset_pin', '0')),
        router_port=get('router', 'port', ''),
        router_host=board('router', 'router'),
        router_password=get('router', 'password', ''),
        ap_ssid=get('router', 'ap_ssid', ''),
        ap_password=get('router', 'ap_password', ''),
        ap_channel=int(get('router', 'ap_channel', '11')),
        ap_channel_other=int(get('router', 'ap_channel_other', '1')),
        ap_bandwidth=int(get('router', 'ap_bandwidth', '20')),
        norouter=norouter,
        nocloud=nocloud,
        wifi_ssid=get('wifi', 'ssid', ''),
        wifi_password=get('wifi', 'password', ''),
        dut_mac=get('dut', 'mac', ''),
        dut_ip=get('dut', 'ip', '192.168.4.100'),
        dut_email=get('dut', 'email', ''),
        receiver_host=here('receiver'),
        receiver_port=int(get('receiver', 'port', '8000')),
        receiver_tls_port=int(get('receiver', 'tls_port', '8443')),
        atboard_port=get('atboard', 'port', ''),
        broker_host=here('broker'),
        broker_port=int(get('broker', 'port', '1883')),
        mqtt_topic=get('broker', 'topic', 'waterius'),
    )
