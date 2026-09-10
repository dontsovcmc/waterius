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


def test_H1_wakeup_period(stand: Stand) -> None:
    """
    Период пробуждения соблюдается: устройство само выходит на связь раз в
    заданные минуты.

    Измеряем по факту, а не по логу: время отмеряет сторожевой таймер attiny, и
    единственное честное доказательство - интервалы между сеансами.

    Режим «только при расходе» здесь не трогаем: к базовому состоянию его
    возвращает ensure_baseline, а на прошивках младше 2.0.47 такой настройки
    нет вовсе.
    """
    stand.setup(period_min=PERIOD_MIN)

    stand.reset_observers()
    stamps = []
    for _ in range(3):
        session = stand.wait_session(timeout=12 * 60, mode=TRANSMIT_MODE)
        stamps.append(time.time())
        assert session.mode == TRANSMIT_MODE

    for before, after in zip(stamps, stamps[1:]):
        minutes = (after - before) / 60
        assert 3 <= minutes <= 7, f'интервал между сеансами {minutes:.1f} мин'


@pytest.mark.requires(esp='2.0.47')       # младшие не печатают период attiny в лог
def test_H1b_period_reaches_attiny(stand: Stand) -> None:
    """
    Заказанный период доезжает до attiny.

    В логе будет 4, а не 5: при смене периода пользователем накопленная поправка
    перестаёт годиться, и прошивка заказывает заведомо меньшее значение -
    проснуться раньше цели не страшно, проснуться позже значит проехать точку.
    """
    session = stand.setup(period_min=PERIOD_MIN)

    assert session.period_attiny is not None, (
        f'в логе нет строки про период attiny\n{session.text}')
    assert 3 <= session.period_attiny <= PERIOD_MIN, (
        f'в attiny уехал период {session.period_attiny}')


@pytest.mark.requires(esp='2.0.47')       # у младших нет режима «только при расходе»
def test_H3_silent_when_no_consumption(stand: Stand) -> None:
    """
    Режим «только при расходе»: без воды устройство просыпается, но молчит.

    Пробуждение при этом есть, и в логе оно выглядит завершённым сеансом
    mode=2 - «сеанса не было» здесь неверно. Проверяется, что в этом сеансе
    не поднималось радио и не ушла посылка.

    Наблюдаем со второго пробуждения: первое после кнопки - разовая передача,
    она выходит на связь всегда.
    """
    stand.setup(period_min=PERIOD_MIN, send_on_consumption=1, channel=1, ctype=NAMUR)
    stand.reset_observers()

    session = stand.wait_session(timeout=3 * PERIOD_MIN * 60, mode=TRANSMIT_MODE)

    idle = session.idle_send
    assert idle is not None, f'нет строки Idle:\n{session.text}'
    assert idle['consumed'] == 0, f'расхода не было, а прошивка его насчитала: {idle}'
    assert idle['transmit'] == 0, f'прошивка решила выйти на связь: {idle}'
    assert 'Idle: no consumption, WiFi stays off' in session.text, session.text
    assert not session.wifi_connected, session.text
    assert session.payload is None, 'без расхода посылки быть не должно'


@pytest.mark.requires(esp='2.0.47')       # у младших нет режима «только при расходе»
def test_H4_consumption_wakes_it_up(stand: Stand) -> None:
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
