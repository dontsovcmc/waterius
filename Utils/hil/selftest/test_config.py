"""
Свой адрес в stand.ini - без железа.

28 сентября прогон умер на первой же фикстуре: «брокер не отвечает на
192.168.100.18:1883». Ни прошивка, ни стенд не были виноваты - DHCP выдал Маку
другой адрес, а в файле остался вчерашний. Приёмник и брокер слушают именно на
этой машине, поэтому их `host` не настройка, а наблюдение: его спрашивают у
ядра перед каждым прогоном.
"""

from __future__ import annotations

import socket
from pathlib import Path
from typing import Any

import pytest

from .. import config as stand_config

СТЕНД = """\
; Стенд Ватериуса.

[metf]
host = 192.168.100.21
button_pin = 1

[router]
host = 192.168.100.20
; Комментарий про host, который править нельзя: host = 10.0.0.1
password = 12345678

[receiver]
; Адрес рабочей машины со стороны проводной сети.
host = 192.168.100.18
port = 8010
tls_port = 8443

[broker]
host = 192.168.100.18
port = 1883
topic = waterius
"""


@pytest.fixture
def файл(tmp_path: Path) -> Path:
    path = tmp_path / 'stand.ini'
    path.write_text(СТЕНД, encoding='utf-8')
    return path


@pytest.fixture
def чужой_адрес(monkeypatch: pytest.MonkeyPatch) -> str:
    """Ядро будто бы ходит в сеть стенда с 192.168.100.11."""
    monkeypatch.setattr(stand_config, 'own_ip', lambda towards: '192.168.100.11')
    return '192.168.100.11'


def test_свой_адрес_спрашивают_у_ядра() -> None:
    """Дорогу к самому себе знает и машина без сети."""
    assert stand_config.own_ip('127.0.0.1') == '127.0.0.1'


def test_адрес_приёмника_и_брокера_берут_у_машины(файл: Path,
                                                  чужой_адрес: str) -> None:
    cfg = stand_config.load(файл)

    assert cfg.receiver_host == чужой_адрес
    assert cfg.broker_host == чужой_адрес
    # Платы стенда стоят на своих адресах: их подменять нечем.
    assert cfg.metf_host == '192.168.100.21'
    assert cfg.router_host == '192.168.100.20'


def test_разошедшуюся_строку_чинят_в_файле(файл: Path, чужой_адрес: str) -> None:
    """
    Иначе следующий прогон начнётся с того же отказа, а человек будет читать
    в файле адрес, по которому ничего не слушает.
    """
    stand_config.load(файл)
    стало = файл.read_text(encoding='utf-8')

    assert стало.count(f'host = {чужой_адрес}') == 2
    assert '192.168.100.18' not in стало


def test_комментарии_переживают_починку(файл: Path, чужой_адрес: str) -> None:
    """configparser.write() вернул бы файл без единого комментария."""
    stand_config.load(файл)
    стало = файл.read_text(encoding='utf-8')

    assert '; Адрес рабочей машины со стороны проводной сети.' in стало
    assert '; Комментарий про host, который править нельзя: host = 10.0.0.1' in стало
    assert 'host = 192.168.100.20' in стало          # роутер на месте


def test_переменная_окружения_главнее(файл: Path, чужой_адрес: str,
                                      monkeypatch: pytest.MonkeyPatch) -> None:
    """Приёмник уводят на другую машину ею, и подмена туда не лезет."""
    monkeypatch.setenv('HIL_RECEIVER_HOST', '10.1.2.3')

    cfg = stand_config.load(файл)

    assert cfg.receiver_host == '10.1.2.3'
    assert cfg.broker_host == чужой_адрес
    assert 'host = 10.1.2.3' not in файл.read_text(encoding='utf-8')


def test_совпавший_адрес_файл_не_трогает(файл: Path,
                                         monkeypatch: pytest.MonkeyPatch) -> None:
    """Обычный прогон не должен шевелить stand.ini и сорить в лог."""
    monkeypatch.setattr(stand_config, 'own_ip', lambda towards: '192.168.100.18')
    было = файл.read_text(encoding='utf-8')

    cfg = stand_config.load(файл)

    assert cfg.broker_host == '192.168.100.18'
    assert файл.read_text(encoding='utf-8') == было


def test_адрес_ищут_со_стороны_сети_стенда(файл: Path,
                                           monkeypatch: pytest.MonkeyPatch) -> None:
    """
    На Маке подняты VPN (utun*) и link-local 169.254.x. Спрашивать надо дорогу
    к роутеру стенда, иначе ответом будет адрес туннеля.
    """
    спросили: list[str] = []
    monkeypatch.setattr(stand_config, 'own_ip',
                        lambda towards: спросили.append(towards) or '192.168.100.11')

    stand_config.load(файл)

    assert спросили and set(спросили) == {'192.168.100.20'}


def test_без_сети_остаётся_написанное(файл: Path,
                                      monkeypatch: pytest.MonkeyPatch) -> None:
    """Отказ сокета - не повод уронить весь прогон на чтении настроек."""
    def нет_сети(towards: str) -> str:
        raise OSError('сеть недоступна')

    monkeypatch.setattr(stand_config, 'own_ip', нет_сети)

    cfg = stand_config.load(файл)

    assert cfg.broker_host == '192.168.100.18'
    assert файл.read_text(encoding='utf-8') == СТЕНД


def test_пустой_файл_не_остаётся_без_адреса(tmp_path: Path,
                                            monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Стенда без stand.ini не бывает, но и тогда адрес - свой, а не пример из
    документации: иначе отказ придёт на десятом тесте, а не на первом.
    """
    monkeypatch.setattr(stand_config, 'own_ip', lambda towards: '192.168.100.11')

    cfg = stand_config.load(tmp_path / 'нет.ini')

    assert cfg.receiver_host == '192.168.100.11'
    assert cfg.broker_host == '192.168.100.11'


def test_сокет_закрывается(monkeypatch: pytest.MonkeyPatch) -> None:
    """Прогон зовёт own_ip дважды за запуск и ещё раз на каждый стенд."""
    открытые: list[Any] = []
    настоящий = socket.socket

    def учёт(*args: Any, **kwargs: Any) -> Any:
        сокет = настоящий(*args, **kwargs)
        открытые.append(сокет)
        return сокет

    monkeypatch.setattr(socket, 'socket', учёт)
    stand_config.own_ip('127.0.0.1')

    assert открытые and all(сокет.fileno() == -1 for сокет in открытые)


@pytest.fixture
def нашлась(monkeypatch: pytest.MonkeyPatch) -> list[tuple[str, str, str]]:
    """Поиск платы, который ничего не делает, но помнит, о чём его спросили."""
    from .. import discover

    звали: list[tuple[str, str, str]] = []

    def find(kind: str, written: str, near: str) -> str:
        звали.append((kind, written, near))
        return '192.168.100.3' if kind == 'metf' else ''

    monkeypatch.setattr(discover, 'find', find)
    return звали


def test_по_умолчанию_платы_не_ищут(файл: Path, чужой_адрес: str,
                                    нашлась: list[Any]) -> None:
    """Разбору настроек сеть не нужна: поиск ходит по ней и стоит секунды."""
    cfg = stand_config.load(файл)

    assert cfg.metf_host == '192.168.100.21'
    assert нашлась == []


def test_с_поиском_адрес_платы_берут_у_найденной(файл: Path, чужой_адрес: str,
                                                 нашлась: list[Any]) -> None:
    cfg = stand_config.load(файл, search=True)

    assert cfg.metf_host == '192.168.100.3'
    assert 'host = 192.168.100.3' in файл.read_text(encoding='utf-8')
    assert ('metf', '192.168.100.21', чужой_адрес) in нашлась


def test_не_нашлась_значит_остаётся_записанное(файл: Path, чужой_адрес: str,
                                               нашлась: list[Any]) -> None:
    """Роутер поиск не нашёл: адрес из файла лучше пустоты - по нему хоть отказ понятен."""
    cfg = stand_config.load(файл, search=True)

    assert cfg.router_host == '192.168.100.20'


def test_переменная_окружения_главнее_поиска(файл: Path, чужой_адрес: str,
                                             нашлась: list[Any],
                                             monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv('HIL_METF_HOST', '10.0.0.7')

    cfg = stand_config.load(файл, search=True)

    assert cfg.metf_host == '10.0.0.7'
    assert [k for k, *_ in нашлась] == ['router']


def test_без_роутера_сеть_устройства_берут_из_wifi(файл: Path, чужой_адрес: str) -> None:
    """Точки стенда нет - имя и пароль сети спросить не у кого, только файл."""
    файл.write_text(СТЕНД + '\n[wifi]\nssid = home\npassword = secret\n',
                    encoding='utf-8')

    cfg = stand_config.load(файл, norouter=True)

    assert (cfg.dut_ssid, cfg.dut_password) == ('home', 'secret')


def test_с_роутером_сеть_устройства_задаёт_точка(файл: Path, чужой_адрес: str) -> None:
    файл.write_text(СТЕНД.replace('password = 12345678',
                                  'password = 12345678\nap_ssid = waterius_stand\n'
                                  'ap_password = standpass')
                    + '\n[wifi]\nssid = home\npassword = secret\n', encoding='utf-8')

    cfg = stand_config.load(файл)

    assert (cfg.dut_ssid, cfg.dut_password) == ('waterius_stand', 'standpass')


def test_без_роутера_плату_точки_не_ищут(файл: Path, чужой_адрес: str,
                                         нашлась: list[Any]) -> None:
    """Обход /24 ради платы, которой нет, стоит секунд и строки лжи в stand.ini."""
    stand_config.load(файл, search=True, norouter=True)

    assert [k for k, *_ in нашлась] == ['metf']
