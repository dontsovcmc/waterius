"""
Две защиты роутера, за которые уже заплачено на соседнем стенде.

Канал выше 11 со страной `01` прошивка принимает молча, кладёт в NVS и при
следующем старте падает в abort() до запуска консолей - плата чинится только
прошивальщиком. А перезагрузка, судимая по живому сокету, объявляется
состоявшейся, даже когда плата и не думала перезагружаться: запись в
полуоткрытый сокет проходит успешно.

Железо не нужно: консоль подменена.
"""

from __future__ import annotations

import pytest

from ..router import MAX_AP_CHANNEL, NatRouter, RouterError


class Console:
    """Консоль, отвечающая по словарю; ответ на `show status` задаёт аптайм."""

    def __init__(self, uptimes: list[str] | None = None) -> None:
        self.sent: list[str] = []
        self._uptimes = uptimes or ['00:10:00']

    def write_line(self, line: str) -> None:
        self.sent.append(line)

    def read_idle(self, timeout: float, idle: float) -> str:
        line = self.sent[-1] if self.sent else ''
        if line == 'show status':
            up = self._uptimes.pop(0) if len(self._uptimes) > 1 else self._uptimes[0]
            return f'{line}\nRouter Status:\nUptime: {up} (since 2026-09-23 13:00:00)\nesp32>'
        return f'{line}\nesp32>'

    def drain(self) -> None:
        pass

    def close(self) -> None:
        pass


def router(uptimes: list[str] | None = None) -> NatRouter:
    return NatRouter(Console(uptimes))


@pytest.mark.parametrize('channel', [12, 13, -1, 99])
def test_запрещённый_канал_не_уходит_в_роутер(channel: int) -> None:
    board = router()
    with pytest.raises(RouterError) as err:
        board.set_ap_channel(channel)
    assert str(MAX_AP_CHANNEL) in str(err.value)
    assert not board._t.sent, 'команда всё-таки ушла на плату'


@pytest.mark.parametrize('channel', [0, 1, 6, MAX_AP_CHANNEL])
def test_разрешённый_канал_уходит(channel: int) -> None:
    board = router()
    board.set_ap_channel(channel)
    assert board._t.sent == [f'set_ap_channel {channel}']


def test_перезагрузка_доказана_упавшим_аптаймом() -> None:
    """Аптайм после команды меньше прежнего - плата действительно стартовала."""
    board = router(['00:10:00', '00:00:05'])
    board.restart(wait=0.0)
    assert 'restart' in board._t.sent


def test_невыполненная_перезагрузка_названа_своим_именем() -> None:
    """Сокет жив, команда легла в буфер, аптайм растёт - это не перезагрузка."""
    board = router(['00:10:00', '00:10:01'])
    with pytest.raises(RouterError, match='не перезагрузился'):
        board.restart(wait=0.0)


def test_аптайм_разбирается() -> None:
    assert router(['01:02:03']).uptime() == pytest.approx(3723.0)
