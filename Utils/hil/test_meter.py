"""
Показания и приросты - блок D ручного плана.

Период на время этих тестов длинный: при коротком плановое пробуждение
приходится ровно посреди серии импульсов, и прирост разъезжается на две
посылки. Это не дефект прошивки, а неверно поставленный опыт.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

NAMUR = 0
ELECTRONIC = 2       # CounterType: импульс - замыкание на минус
ELECTRONIC_HIGH = 4  # CounterType: импульс - подъём линии
WATER_COLD = 0
ELECTRO = 2          # CounterName: у электричества вес - импульсы на кВт*ч
FACTOR = 10          # л/имп
# Столько, чтобы прирост был заведомо не нулевым и не совпал ни с числом
# импульсов, ни с весом: 3 x 10 = 30 литров. Больше не доказывает ничего -
# арифметика в прошивке одна и та же на трёх импульсах и на трёхстах.
PULSES = 3

# Импульс электронного выхода короткий: у газового счётчика Гранд 0,7-1,5 мс.
# Миллисекунда - предел стенда: выдержку отмеряет плата, а заказывается она в
# миллисекундах
SHORT_PULSE_MS = 1

# Положительный импульс стенд отмеряет с компьютера, поэтому он длинный.
# Длину проверяет соседний тест, здесь важна полярность
HIGH_PULSE_MS = 30


def test_D1_delta_matches_pulses(stand: Stand) -> None:
    """Импульсы при весе 10 л/имп дают ровно десять литров каждый."""
    before = stand.setup(channel=1, factor=FACTOR, ctype=NAMUR, period_min=120)
    assert before.payload is not None
    ch_before = float(before.payload['ch1'])

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()

    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Литры целые - сравниваем точно, кубометры дробные - с допуском
    session.assert_delta(channel=1, liters=PULSES * FACTOR)
    cubic = PULSES * FACTOR / 1000
    assert abs(float(session.payload['ch1']) - ch_before - cubic) < 0.001


@pytest.mark.slow
def test_D2a_missed_session_keeps_consumption(stand: Stand) -> None:
    """
    Пропущенный сеанс не теряет расход.

    Точка отсчёта двигается в конце сеанса, а он выполняется только при
    поднятом Wi-Fi. Значит при выключенной точке доступа импульсы обоих
    периодов приедут одной посылкой.
    """
    stand.setup(channel=1, factor=FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    with stand.net.ap_off():
        stand.dut.pulse(channel=1, count=PULSES)
        stand.dut.press_button()
        missed = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert not missed.wifi_connected

    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=2 * PULSES * FACTOR)


@pytest.mark.slow
@pytest.mark.requires(esp='2.0.47')       # младшие двигают точку отсчёта после коннекта
def test_D2b_undelivered_session_keeps_delta(stand: Stand) -> None:
    """
    Тот же опыт, но сеть жива, а получатели недоступны.

    Прирост не теряется от того, что посылка не доехала: точка отсчёта
    двигается после доставки, а не после подключения к Wi-Fi.
    """
    stand.setup(channel=1, factor=FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    with stand.net.internet_down():
        stand.dut.pulse(channel=1, count=PULSES)
        stand.dut.press_button()
        missed = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert missed.wifi_connected, 'Wi-Fi должен был подняться'

    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=2 * PULSES * FACTOR)


def test_D3_electricity_counts_kilowatt_hours(stand: Stand) -> None:
    """
    У электричества вес импульса - это импульсы на кВт*ч, то есть делитель, а
    не множитель (`core/readings.cpp`). Десять импульсов при весе 10 дают ровно
    один киловатт-час.

    Дробная часть прироста при этом теряется: `delta` уезжает в посылку целым
    числом. Проверяем именно так, как оно есть, - на целом киловатт-часе.
    """
    per_kwh = 10
    try:
        before = stand.setup(channel=1, cname=ELECTRO, ctype=NAMUR,
                             factor=per_kwh, value='100')
        assert before.payload is not None
        kwh_before = float(before.payload['ch1'])

        stand.reset_observers()
        stand.dut.pulse(channel=1, count=per_kwh)
        stand.dut.press_button()
        session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

        assert abs(float(session.payload['ch1']) - kwh_before - 1.0) < 0.001, (
            f"было {kwh_before}, стало {session.payload['ch1']}")
        session.assert_delta(channel=1, liters=1)
    finally:
        stand.setup(channel=1, cname=WATER_COLD, factor=FACTOR, value='20.000')


def test_D4_readings_below_current_are_accepted(stand: Stand) -> None:
    """
    Показания меньше текущих: точка отсчёта переписывается, показания не уходят
    в минус.

    Так выглядит и обычная ошибка ввода, и замена счётчика на новый с нулём на
    табло: прошивка обязана поверить человеку, а не своей истории.
    """
    stand.setup(channel=1, ctype=NAMUR, factor=FACTOR, value='500.000')
    lowered = stand.setup(channel=1, value='1.000')

    assert lowered.payload is not None
    assert abs(float(lowered.payload['ch1']) - 1.0) < 0.001, (
        f"показания не сбросились: {lowered.payload['ch1']}")

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Счёт продолжается от введённого числа, а не от прежнего
    expected = 1.0 + PULSES * FACTOR / 1000
    assert abs(float(session.payload['ch1']) - expected) < 0.001, (
        f"ждали {expected}, приехало {session.payload['ch1']}")
    session.assert_delta(channel=1, liters=PULSES * FACTOR)


def test_D5_electronic_input_catches_short_pulses(stand: Stand) -> None:
    """
    Электронный вход считает импульс длиной в миллисекунду.

    Уровень снимается в прерывании по фронту и защёлкивается
    (`Attiny85/src/electronic.h`): главный цикл attiny за миллисекунду до входа
    не доходит - он спит, пишет EEPROM и общается с ЕСП. Само правило
    проверяют хостовые тесты, здесь - живой пин, настоящее прерывание и
    настоящая длительность.

    Тип «Электронный (+)» так не проверить: у стенда сухой контакт, а не
    источник уровня (04_not-tested.md).
    """
    try:
        stand.setup(channel=1, ctype=ELECTRONIC, factor=FACTOR)
        stand.reset_observers()

        stand.dut.pulse(channel=1, count=PULSES, width_ms=SHORT_PULSE_MS)
        stand.dut.press_button()
        session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

        # Ровно столько, сколько подали: ни потерянных, ни удвоенных дребезгом
        session.assert_delta(channel=1, liters=PULSES * FACTOR)
    finally:
        stand.setup(channel=1, ctype=NAMUR)


def test_D6_electronic_high_input_counts_rising_pulses(stand: Stand) -> None:
    """
    Тип «Электронный (+)»: импульс - подъём линии, а не замыкание.

    Подтяжку attiny в этом типе выключает (`counter.h`, set_type), уровень
    задаёт счётчик - и в опыте его задаёт стенд: в покое линия прижата к
    земле, импульс - короткий подъём. Отпущенную линию высокоомный вход ловил
    бы как эфир.

    Идёт только там, где вход заведён на плату так, что ей можно выдавать
    уровень: без последовательного резистора плата упирает 3,3 В в вход,
    питающийся от батареек.
    """
    if not stand.cfg.can_drive_high:
        pytest.skip('стенд не выдаёт уровень на вход: [metf] can_drive_high в '
                    'stand.ini, обвязка - 04_not-tested.md')

    try:
        stand.setup(channel=1, ctype=ELECTRONIC_HIGH, factor=FACTOR)
        with stand.dut.driven(channel=1):
            stand.reset_observers()
            stand.dut.pulse_high(channel=1, count=PULSES, width_ms=HIGH_PULSE_MS)
            stand.dut.press_button()
            session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

        session.assert_delta(channel=1, liters=PULSES * FACTOR)
    finally:
        stand.setup(channel=1, ctype=NAMUR)
