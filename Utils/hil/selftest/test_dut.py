"""
Паузы между импульсами не оставляют лог устройства без присмотра.

Железо не нужно: плату заменяет заглушка, а часами управляет тест. Проверяется
то, из-за чего падал E2: пока пауза была одним `sleep`, устройство успевало
проснуться по расписанию, напечатать сотни строк, и кольцо METF (511 строк,
полтора сеанса) переполнялось. Тест падал не на своей проверке, а на «лог
неполон, утверждать по нему нечего».
"""

from __future__ import annotations

import pytest

from .. import dut as dut_mod


class Clock:
    """Часы под управлением теста: `sleep` двигает время, а не ждёт."""

    def __init__(self) -> None:
        self.now = 1000.0

    def time(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeApi:
    """Плата, которая молча принимает всё: нас интересуют только паузы."""

    def __init__(self, clock: Clock) -> None:
        self.clock = clock
        self.lows: list[float] = []       # когда прижималась линия

    def pinMode(self, pin: int, mode: int) -> None:      # noqa: N802
        pass

    def digitalWrite(self, pin: int, value: int) -> None:  # noqa: N802
        if value == dut_mod.LOW:
            self.lows.append(self.clock.now)

    def __getattr__(self, name: str):                    # rgb_*, pulse и прочее
        raise AttributeError(name)


@pytest.fixture
def clock(monkeypatch: pytest.MonkeyPatch) -> Clock:
    fake = Clock()
    monkeypatch.setattr(dut_mod, 'time', fake)
    return fake


def make(clock: Clock, idle=None, settle=None) -> tuple[dut_mod.Dut, FakeApi]:
    api = FakeApi(clock)
    board = dut_mod.Dut(api, button_pin=1, ch0_pin=2, ch1_pin=3, reset_pin=4,
                        idle=idle, settle=settle)
    board._led_ok = False                 # индикатора у заглушки нет
    return board, api


def test_пауза_вычитывает_лог(clock: Clock) -> None:
    """Минута между импульсами - это десятки обращений к плате, а не одно."""
    reads: list[float] = []
    board, _ = make(clock, idle=lambda: reads.append(clock.now))

    board.pulses(channel=0, count=2, gap=60.0, width_ms=500)

    assert len(reads) >= 50, (
        f'за минуту паузы лог вычитан {len(reads)} раз: кольцо METF вмещает '
        'полтора сеанса и переполнится плановым пробуждением')


def test_ритм_импульсов_не_плывёт(clock: Clock) -> None:
    """
    Вычитывание не имеет права сдвинуть импульс: ритм сравнивается с порогом
    тревоги. Дедлайн абсолютный, поэтому съеденное время не накапливается.
    """
    board, api = make(clock, idle=lambda: clock.sleep(0.015))  # опрос платы

    board.pulses(channel=0, count=4, gap=60.0, width_ms=500)

    starts = api.lows
    assert len(starts) == 4
    for i, moment in enumerate(starts):
        drift = abs(moment - (starts[0] + i * 60.0))
        assert drift < 0.5, f'импульс {i} уехал на {drift:.3f} с'


def test_без_вычитывателя_ведёт_себя_как_раньше(clock: Clock) -> None:
    """Dut портала создаётся без лога - для него ничего не изменилось."""
    board, api = make(clock, idle=None)

    board.pulses(channel=0, count=2, gap=60.0, width_ms=500)

    assert [round(t - api.lows[0]) for t in api.lows] == [0, 60]


def test_выдержка_идёт_до_нажатия(clock: Clock) -> None:
    """
    Нажатие вплотную к концу сеанса до устройства не доходит (механизм не
    установлен, долг в README). Выдержку отмеряет стенд, но ждать она обязана
    до того, как линия прижата, - иначе ждать уже нечего.
    """
    было: list[str] = []
    board, api = make(clock, settle=lambda: было.append(f'выдержка {clock.now}'))

    board.press_button()

    assert было, 'выдержки перед нажатием не было'
    прижали = api.lows[0]
    выждали = float(было[0].split()[1])
    assert выждали <= прижали, 'выдержка кончилась позже, чем прижали линию'


def test_без_выдержки_нажатие_работает_как_прежде(clock: Clock) -> None:
    """Стенд может не давать выдержки вовсе: тогда нажатие идёт сразу."""
    board, _ = make(clock)
    board.press_button()                # не должно упасть
