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
FACTOR = 10          # л/имп
PULSES = 10


def test_D1_delta_matches_pulses(stand: Stand) -> None:
    """Десять импульсов при весе 10 - это ровно сто литров прироста."""
    before = stand.setup(channel=1, factor=FACTOR, ctype=NAMUR, period_min=120)
    assert before.payload is not None
    ch_before = float(before.payload['ch1'])

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()

    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Литры целые - сравниваем точно; кубометры дробные - с допуском
    session.assert_delta(channel=1, liters=PULSES * FACTOR)
    assert abs(float(session.payload['ch1']) - ch_before - 0.1) < 0.001


@pytest.mark.slow
def test_D2a_missed_session_keeps_consumption(stand: Stand, slow_clock: None) -> None:
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
@pytest.mark.xfail(reason='точка отсчёта двигается после коннекта, а не после доставки')
def test_D2b_undelivered_session_keeps_delta(stand: Stand, slow_clock: None) -> None:
    """
    Тот же опыт, но сеть жива, а получатели недоступны.

    Ожидаем то же самое: прирост не должен теряться от того, что посылка не
    доехала. Сейчас `update_config` сдвигает точку отсчёта сразу после
    подключения к Wi-Fi, поэтому расход первого периода пропадает.
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
