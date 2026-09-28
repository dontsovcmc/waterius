"""
Как стенд ловит переезд точки портала на канал роутера - без железа.

Прошивка о переезде не печатает ничего (`active_point.cpp`, ap_channel: точка
поднимается один раз и уходит за радио молча), поэтому единственная улика -
эфир: имя точки должно появиться на канале роутера. Один скан этого не
доказывает - пока ЕСП подключается, её точка то пропадает, то возвращается на
старом канале.
"""

from __future__ import annotations

import pytest

from ..atboard import AtError, Network
from ..test_wizard import portal_in_air

PORTAL = 'waterius-6827706-2.0.51'
OLD, NEW = 11, 1


class ПлатаСоСканами:
    """Плата, отдающая заготовленные сканы по одному."""

    def __init__(self, *сканы: list[Network] | AtError) -> None:
        self.сканы = list(сканы)
        self.вызовов = 0

    def scan(self, ssid: str | None = None, timeout: float = 30.0) -> list[Network]:
        self.вызовов += 1
        снимок = self.сканы.pop(0) if self.сканы else []
        if isinstance(снимок, AtError):
            raise снимок
        return снимок


def точка(channel: int, ssid: str = PORTAL) -> Network:
    return Network(ssid=ssid, rssi=-50, bssid='aa:bb:cc:dd:ee:ff', channel=channel)


@pytest.fixture(autouse=True)
def без_пауз(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr('hil.test_wizard.time.sleep', lambda _: None)


def test_переезд_виден_не_первым_сканом() -> None:
    """Пока ЕСП подключается, точка стоит на старом канале - это не отказ."""
    board = ПлатаСоСканами([точка(OLD)], [], [точка(NEW)])

    moved, air = portal_in_air(board, PORTAL, NEW, timeout=10)

    assert moved is not None and moved.channel == NEW
    assert board.вызовов == 3
    assert [net.channel for net in air] == [NEW]


def test_точка_осталась_на_старом_канале() -> None:
    """
    Переезда не было. Отдать надо и эфир: без него отказ не скажет, где точка
    осталась, и «не переехала» будет не отличить от «пропала совсем».
    """
    board = ПлатаСоСканами([точка(OLD)], [точка(OLD)])

    moved, air = portal_in_air(board, PORTAL, NEW, timeout=0.5)

    assert moved is None
    assert [(net.ssid, net.channel) for net in air] == [(PORTAL, OLD)]


def test_чужая_точка_на_нужном_канале_не_считается() -> None:
    """На канал роутера сама сеть роутера и стоит - спутать её с портальной нельзя."""
    board = ПлатаСоСканами([точка(NEW, ssid='waterius_stand')])

    moved, _ = portal_in_air(board, PORTAL, NEW, timeout=0.5)

    assert moved is None


def test_осечка_скана_не_роняет_ожидание() -> None:
    """Скан во время подключения ЕСП отвечает `ERROR`: это повод повторить."""
    board = ПлатаСоСканами(AtError('скан эфира не удался'), [точка(NEW)])

    moved, _ = portal_in_air(board, PORTAL, NEW, timeout=10)

    assert moved is not None and moved.channel == NEW
