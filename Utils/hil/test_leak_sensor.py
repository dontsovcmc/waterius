"""
Датчик протечки - вход типа LEAKAGE и LEAKAGE_NC (E4a, E5, E12).

Не путать с протечкой по расходу (E3, `test_alarms.py`): там вода течёт через
счётчик и не останавливается, и тревогу считают по импульсам. У датчика
импульсов нет вовсе: тревога - само состояние линии, замкнутой водой на полу
(`Attiny85/src/main.cpp`, alarm_tick). Потому и реакция за секунду, а не на
следующем импульсе или пробуждении.

Тесты, где датчик - лишь самый быстрый способ поднять любую тревогу (доставка
F1-F4, снятие E13 и E17), остались при своих темах: предмет
проверки там не датчик. Публикация датчика в Home Assistant - I2 в
`test_mqtt.py`.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest

from .constants import (ALARM_WAIT_S, LEAKAGE, LEAKAGE_NC, NAMUR, PLANNED_PERIOD_MIN,
                        PLANNED_WAIT_S, SILENCE_S)
from .logwatch import ALARM_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

# Тревог до attiny 41 не существует: alarm_bits всегда 0
pytestmark = [pytest.mark.stand, pytest.mark.requires(attiny=41)]


@pytest.mark.needs(ctype0=LEAKAGE)
def test_E4a_sensor_bounce_gives_one_session(stand: Stand, quiet: None) -> None:
    """
    Дребезг датчика больше не стоит сеансов.

    `set_wet` только поднимает, поэтому намок-высох-намок даёт одну тревогу и
    один сеанс. Раньше каждый переход был новостью, и мокрый ковёр у порога
    будил устройство до исчерпания бюджета.
    """
    stand.reset_observers()

    try:
        stand.dut.wet(channel=0, closed=True)
        stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE).assert_alarm(wet0=1)

        stand.reset_observers()
        for _ in range(3):
            # Секунда на переход: вход опрашивается раз в 250 мс, и мгновенное
            # переключение attiny могла бы не увидеть вовсе
            stand.dut.wet(channel=0, closed=False)
            time.sleep(1.0)
            stand.dut.wet(channel=0, closed=True)
            time.sleep(1.0)

        # Признак новости у attiny не протухает: будь дребезг новостью, сеанс
        # пришёл бы сразу по истечении ALARM_HOLD_MIN, то есть внутри окна.
        stand.expect_no_session(timeout=SILENCE_S, mode=ALARM_MODE)
    finally:
        stand.dut.wet(channel=0, closed=False)


def test_E5_normally_closed_sensor_detects_cut_wire(stand: Stand, quiet: None) -> None:
    """
    Нормально-замкнутый датчик: обрыв провода - это тревога.

    Ради этого он и нужен: у нормально-разомкнутого перекушенный провод выглядит
    как «всё в порядке», и владелец считает себя защищённым.
    """
    stand.dut.wet(channel=0, closed=True)        # спокойное состояние - замкнуто
    stand.setup(channel=0, ctype=LEAKAGE_NC)
    stand.reset_observers()

    stand.dut.wet(channel=0, closed=False)       # обрыв
    session = stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
    session.assert_alarm(wet0=1)

    stand.dut.wet(channel=0, closed=True)


@pytest.mark.slow
@pytest.mark.requires(attiny=43)
@pytest.mark.needs(ctype0=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
def test_E12_type_change_clears_alarm(stand: Stand, quiet: None) -> None:
    """
    Смена типа входа снимает тревогу канала.

    Датчик остаётся замкнутым: тревога описывала прежний вход, и снять её после
    смены типа было бы нечем - опрашивается вход, только пока его тип датчик
    (`Attiny85/src/main.cpp`, alarm_tick).

    Тип меняется плановым сеансом, без кнопки: кнопка снимает тревоги сама, и
    тест зеленел бы на любой прошивке. Отсюда короткий период и метка slow.

    Снятие проверяется следующим сеансом, а не тем, в котором сменили тип: ЕСП
    читает состояние тревог один раз, в начале сеанса, и повторная посылка
    после применения настроек собирается из того же снимка.
    """
    stand.reset_observers()

    try:
        stand.dut.wet(channel=0, closed=True)
        # Любой сеанс: плановый, пришедший до конца паузы attiny после чужой
        # тревоги, увозит эту сам, и тревожного уже не будет
        alarm = stand.wait_session(timeout=ALARM_WAIT_S)
        alarm.assert_alarm(wet0=1)

        stand.setup(channel=0, ctype=NAMUR, wake=False, timeout=PLANNED_WAIT_S)

        stand.reset_observers()
        cleared = stand.wait_session(timeout=PLANNED_WAIT_S)
        cleared.assert_alarm(wet0=0)
    finally:
        stand.dut.wet(channel=0, closed=False)
