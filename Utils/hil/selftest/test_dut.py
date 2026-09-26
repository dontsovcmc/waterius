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
from ..dut import BUTTON_SETUP_MS, BUTTON_SHORT_MS


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
    board, api = make(clock, settle=lambda _: было.append(f'выдержка {clock.now}'))

    board.press_button()

    assert было, 'выдержки перед нажатием не было'
    прижали = api.lows[0]
    выждали = float(было[0].split()[1])
    assert выждали <= прижали, 'выдержка кончилась позже, чем прижали линию'


def test_без_выдержки_нажатие_работает_как_прежде(clock: Clock) -> None:
    """Стенд может не давать выдержки вовсе: тогда нажатие идёт сразу."""
    board, _ = make(clock)
    board.press_button()                # не должно упасть


def test_выдержка_знает_длительность_нажатия(clock: Clock) -> None:
    """
    Длинным нажатием открывают портал, коротким заказывают сеанс, и стенду
    надо их различать: приговор «не проснулся за 2 с» относится только ко
    второму. Раньше выдержка не знала, какое нажатие идёт, и стенд мерил
    двумя секундами открытие портала - так падал test_E19.
    """
    длительности: list[int] = []
    board, _ = make(clock, settle=длительности.append)

    board.press_button()
    board.hold_button()

    assert длительности == [BUTTON_SHORT_MS, BUTTON_SETUP_MS], длительности


class PulsingApi(FakeApi):
    """Плата, умеющая `POST /pulse`: выдержку отмеряет она сама."""

    def __init__(self, clock: Clock) -> None:
        super().__init__(clock)
        self.pulses: list[tuple[int, int, bool]] = []   # вывод, мс, ждали ли конца

    def pulse(self, pin: int, value: int, duration_ms: int,
              wait: bool = True) -> None:
        self.pulses.append((pin, duration_ms, wait))
        if wait:
            self.clock.sleep(duration_ms / 1000.0)


def test_удержание_не_ждёт_конца_замыкания(clock: Clock) -> None:
    """
    Внутри контекста время стоять не должно: там воздействуют на другой вход,
    пока этот замкнут.
    """
    api = PulsingApi(clock)
    board = dut_mod.Dut(api, button_pin=1, ch0_pin=2, ch1_pin=3, reset_pin=4)
    board._led_ok = False

    начало = clock.now
    with board.holding(channel=0, msec=2000) as отпустят:
        assert clock.now == начало, 'удержание съело время, которое нужно опыту'
        assert clock.now < отпустят, 'окно замыкания кончилось до воздействия'
        board.pulse(channel=1, count=1, width_ms=1)

    assert api.pulses[0] == (2, 2000, False), api.pulses
    assert api.pulses[1] == (3, 1, True), api.pulses


def test_удержание_досыпает_остаток(clock: Clock) -> None:
    """
    Выход из контекста обязан дождаться конца выдержки: иначе следующее
    замыкание придёт на ещё занятый вывод, и плата откажет кодом 409.
    """
    api = PulsingApi(clock)
    board = dut_mod.Dut(api, button_pin=1, ch0_pin=2, ch1_pin=3, reset_pin=4)
    board._led_ok = False

    начало = clock.now
    with board.holding(channel=0, msec=2000):
        clock.sleep(0.5)

    assert clock.now == начало + 2.0, f'вышли на {clock.now - начало} с'


def test_удержание_без_слотов_платы_держит_линию_само(clock: Clock) -> None:
    """Старому клиенту без `/pulse` линию прижимает стенд и сам отпускает."""
    board, api = make(clock)          # FakeApi без метода pulse

    with board.holding(channel=0, msec=2000):
        assert api.lows, 'линию не прижали'

    assert clock.now == 1002.0
