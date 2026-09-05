"""
Расписание и режим «только при расходе» - блок H ручного плана.

Все проверки здесь диапазонные. Время отмеряет сторожевой таймер attiny,
частота которого гуляет от экземпляра к экземпляру и от температуры, а ЕСП
компенсирует это поправкой. Требовать точных минут - значит завести мигающий
тест на исправной прошивке.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE, TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = [pytest.mark.stand, pytest.mark.slow]

NAMUR = 0
PERIOD_MIN = 5


def test_H1_wakeup_period(stand: Stand, slow_clock: None) -> None:
    """
    Период пробуждения соблюдается.

    В логе будет 4, а не 5: при смене периода пользователем накопленная поправка
    перестаёт годиться, и прошивка заказывает заведомо меньшее значение -
    проснуться раньше цели не страшно, проснуться позже значит проехать точку.
    """
    first = stand.setup(period_min=PERIOD_MIN, send_on_consumption=0)
    assert first.period_attiny is not None
    assert 3 <= first.period_attiny <= PERIOD_MIN, (
        f'в attiny уехал период {first.period_attiny}')

    stand.reset_observers()
    stamps = []
    for _ in range(3):
        session = stand.wait_session(timeout=12 * 60, mode=TRANSMIT_MODE)
        stamps.append(time.time())
        assert session.mode == TRANSMIT_MODE

    for before, after in zip(stamps, stamps[1:]):
        minutes = (after - before) / 60
        assert 3 <= minutes <= 7, f'интервал между сеансами {minutes:.1f} мин'


def test_H3_silent_when_no_consumption(stand: Stand, slow_clock: None) -> None:
    """
    Режим «только при расходе»: без воды устройство просыпается, но молчит.

    Наблюдать надо со второго пробуждения: первое после кнопки - разовая
    передача, она выходит на связь всегда.
    """
    stand.setup(period_min=PERIOD_MIN, send_on_consumption=1, channel=1, ctype=NAMUR)
    stand.reset_observers()

    stand.expect_no_session(timeout=3 * PERIOD_MIN * 60, mode=TRANSMIT_MODE)

    stand.log.poll()
    text = '\n'.join(stand.log.lines)
    assert 'Idle: no consumption, WiFi stays off' in text, (
        'устройство должно было просыпаться и засыпать молча')
    assert 'WIFI: Connecting...' not in text


def test_H4_consumption_wakes_it_up(stand: Stand, slow_clock: None) -> None:
    """Импульс возвращает связь на ближайшем плановом пробуждении."""
    stand.setup(period_min=PERIOD_MIN, send_on_consumption=1, channel=1, ctype=NAMUR)
    stand.reset_observers()

    stand.dut.pulse(channel=1, count=2)

    session = stand.wait_session(timeout=3 * PERIOD_MIN * 60, mode=TRANSMIT_MODE)
    idle = session.idle_send
    assert idle is not None, f'нет строки Idle:\n{session.text}'
    assert idle['consumed'] == 1
    assert idle['transmit'] == 1
    assert session.payload is not None and session.payload['delta1'] > 0
