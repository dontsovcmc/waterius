"""
Режим «Я уехал» (#88) - E6, E10, E18 ручного плана.

Отпуск - не отдельная тревога, а подмена порога: пока режим включён, порог
объёма для attiny - один импульс, и тревогой становится любой расход
(`core/alarm.cpp`, alarm_thresholds). Отсюда три свойства: тревога на первом
же импульсе (ею начинаются оба теста), работа без заданного веса импульса
(E10) и снятие своей тревоги при выключении режима (E18; `active_point_api.cpp`,
save_vacation).

Подменённый порог в attiny (`vol1=1`, `vacation=1`) проверяет I3 в
`test_mqtt_alarms.py`, заодно с доставкой команды из Home Assistant.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import pytest

from .constants import (ALARM_WAIT_S, AUTO_IMPULSE_FACTOR, BASE_FACTOR, NAMUR,
                        PLANNED_PERIOD_MIN, PLANNED_WAIT_S, RESET_FLOW1,
                        VACATION_PULSES, VOL_LITRES, VOL_PULSES)
from .logwatch import ALARM_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

# Тревог до attiny 41 не существует: alarm_bits всегда 0
pytestmark = [pytest.mark.stand, pytest.mark.requires(attiny=41)]


@pytest.mark.slow
def test_E18_vacation_off_clears_its_alarm(stand: Stand, quiet: None) -> None:
    """
    Выключение режима снимает тревогу, которую режим и поднял.

    Иначе она висела бы после возвращения: порог в режиме подменён одним
    импульсом, и сработать он был обязан. Снимается только ALARM_FLOW обоих
    каналов - протечка и датчик к режиму отношения не имеют.

    Режим выключается плановым сеансом: кнопка сняла бы тревогу и без него.
    """
    stand.setup_alarms(channel=1, factor=BASE_FACTOR, alarm_vol=VOL_LITRES,
                       ctype=NAMUR, vacation=0, period_min=PLANNED_PERIOD_MIN)
    stand.setup(vacation=1)

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=1)
    # Любой сеанс: при периоде 5 минут плановый может увезти тревогу раньше
    # тревожного, если attiny ещё держит паузу после прошлой тревоги
    stand.wait_session(timeout=ALARM_WAIT_S).assert_alarm(flow1=1)

    off = stand.setup(vacation=0, wake=False, timeout=PLANNED_WAIT_S)
    assert off.alarm_config['vacation'] == 0
    assert off.alarm_config['vol1'] == VOL_PULSES, 'пороги пользователя должны вернуться'
    assert off.alarm_config['reset'] & RESET_FLOW1, (
        f'выключение режима обязано снять свою тревогу: {off.alarm_config}')

    stand.reset_observers()
    stand.wait_session(timeout=PLANNED_WAIT_S).assert_alarm(flow1=0)


@pytest.mark.reset
def test_E10_vacation_works_without_factor(stand: Stand, fresh_device: Any) -> None:
    """
    Режиму «Я уехал» известный вес импульса не нужен.

    Обычный порог без веса посчитать нельзя - литры на импульс взять неоткуда,
    и пересчёт вернёт ноль, то есть «выключено». Отпуску считать нечего:
    тревогой объявлен любой импульс, и порог объёма подменяется единицей.
    Поэтому в `alarm_thresholds` (`core/alarm.cpp`) vacation проверяется до
    веса, и тест следит именно за этим порядком.

    Вес не задан только после заводского сброса: настройками тройку не
    сохранить, «Авто» разворачивается в число в момент применения.
    """
    session = fresh_device.leave()
    assert session.payload['f1'] == AUTO_IMPULSE_FACTOR, (
        'тест бессмысленен, если вес всё-таки задан')

    stand.setup(channel=1, ctype=NAMUR)
    on = stand.setup(vacation=1)
    assert on.payload['f1'] == AUTO_IMPULSE_FACTOR, 'вес не должен был появиться'
    assert on.alarm_config['vol1'] == VACATION_PULSES, (
        f'порог отпуска не уехал в attiny: {on.alarm_config}\n{on.text}')

    stand.reset_observers()
    # Минута между импульсами - расход, который ни один обычный порог тревогой
    # не считает. В отпуске считается любой.
    stand.dut.pulses(channel=1, count=2, gap=60.0)

    stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE).assert_alarm(flow1=1)
