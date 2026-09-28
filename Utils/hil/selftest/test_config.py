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
