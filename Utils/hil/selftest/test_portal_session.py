"""
Выход из портала после теста не повторяет того, что тест уже сделал сам.

W1 закрывает портал сам и сам дожидается сеанса после выхода. Выход фикстуры
слал `/api/turnoff` второй раз - в точку, которой уже нет, - и минуту ждал сна
устройства, которое давно спало: около 80 с на каждом прогоне и два ложных
предупреждения («портал не закрылся командой», «устройство не уснуло за 60 с»).
"""

from __future__ import annotations

from typing import Any

import pytest

from .. import portal as portal_mod
from ..atboard import AtError


class Плата:
    """AT-плата, у которой спрашивают только выход из портала."""

    def __init__(self, отвечает: bool = True) -> None:
        self.отвечает = отвечает
        self.запросы: list[str] = []
        self.closed_at: float | None = None

    def get(self, path: str, host: str) -> None:
        self.запросы.append(path)
        if not self.отвечает:
            raise AtError('станция вне сети')

    def close(self) -> None:
        pass


class Стенд:
    """Стенд с часами последнего сеанса и счётом ожиданий сна."""

    def __init__(self) -> None:
        self.сеанс_в = 0.0
        self.ждали_сна = 0

    def slept_since(self, moment: float | None) -> bool:
        return moment is not None and self.сеанс_в > moment

    def wait_asleep(self) -> bool:
        self.ждали_сна += 1
        return True


@pytest.fixture
def плата(monkeypatch: pytest.MonkeyPatch) -> Плата:
    board = Плата()
    monkeypatch.setattr(portal_mod, 'open_portal', lambda *args, **kwargs: board)
    return board


def test_портал_без_выхода_закрывает_фикстура(плата: Плата) -> None:
    стенд = Стенд()
    with portal_mod.session(None, стенд):
        pass
    assert плата.запросы == ['/api/turnoff']
    assert стенд.ждали_сна == 1


def test_тест_закрыл_и_дождался_сеанса(плата: Плата) -> None:
    стенд = Стенд()
    with portal_mod.session(None, стенд) as board:
        portal_mod.turnoff(board)
        стенд.сеанс_в = board.closed_at + 1.0       # wait_session после выхода
    assert плата.запросы == ['/api/turnoff']
    assert стенд.ждали_сна == 0


def test_тест_закрыл_но_упал_до_сеанса(плата: Плата) -> None:
    """Сеанс после выхода ещё идёт: пока ЕСП запитана, кнопка следующего теста не дойдёт."""
    стенд = Стенд()

    def мастер() -> None:
        with portal_mod.session(None, стенд) as board:
            portal_mod.turnoff(board)
            raise AssertionError('мастер сломался')

    with pytest.raises(AssertionError, match='мастер сломался'):
        мастер()
    assert плата.запросы == ['/api/turnoff']
    assert стенд.ждали_сна == 1


def test_неотвеченный_выход_повторяется(monkeypatch: pytest.MonkeyPatch) -> None:
    """Ответ не дошёл - неизвестно, закрыт ли портал: фикстура шлёт выход ещё раз."""
    board: Any = Плата(отвечает=False)
    monkeypatch.setattr(portal_mod, 'open_portal', lambda *args, **kwargs: board)
    стенд = Стенд()
    with portal_mod.session(None, стенд) as device:
        portal_mod.turnoff(device)
    assert device.closed_at is None
    assert board.запросы == ['/api/turnoff', '/api/turnoff']
    assert стенд.ждали_сна == 1
