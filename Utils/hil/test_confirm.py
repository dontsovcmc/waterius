"""
Кому доклад о тревоге обязан доехать - блок F ручного плана.

Все четыре теста читают одну строку лога:

    Alarm confirm: mask=4 waterius=1 http=0 mqtt=3 any=1 -> 0

Утверждать надо её целиком, а не только итог. Первоначальная версия проверяла
`-> 1`, и это ничего не значило: единица печатается при любой маске и любом
удачном сеансе, то есть тест был зелёным даже при полностью выключенных
тревогах. Различить F2 и F3 тоже можно лишь по одному числу: mqtt=3 - брокер
недоступен, mqtt=0 - получатель выключен и из условия выпадает.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest

from .logwatch import (ALARM_MODE, SEND_NO_CONNECTION, SEND_OK, SEND_SKIPPED)
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

NAMUR = 0
FACTOR = 10
FLOW_THRESHOLD = 3600

CONFIRM_MQTT = 4        # AlarmConfirm, core/types.h


def arm_alarms(stand: Stand, **extra: int) -> None:
    """Общее предусловие: пороги в attiny, режим отпуска выключен."""
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=FLOW_THRESHOLD,
                       ctype=NAMUR, vacation=0, **extra)


def test_F1_any_receiver_is_enough(stand: Stand, quiet: None) -> None:
    """Маска пустая: хватает любого получателя, повторных сеансов нет."""
    arm_alarms(stand, confirm_waterius=0, confirm_http=0, confirm_mqtt=0)
    stand.reset_observers()

    stand.dut.pulses(channel=1, count=2, gap=3.0)

    session = stand.wait_session(timeout=120, mode=ALARM_MODE)
    session.assert_alarm(flow1=1)
    session.assert_confirm(mask=0, any=1, confirmed=1)

    # Квитанция ушла, значит будить нас больше незачем
    stand.expect_no_session(timeout=900, mode=ALARM_MODE)


@pytest.mark.slow
def test_F2_required_receiver_unreachable(stand: Stand, quiet: None,
                                          slow_clock: None) -> None:
    """
    MQTT отмечен обязательным и недоступен: квитанции нет, attiny будит ЕСП
    снова. Брокер при этом остаётся включённым в настройках - иначе получится
    совсем другой сценарий, F3.
    """
    arm_alarms(stand, confirm_mqtt=1)
    stand.reset_observers()

    with stand.net.mqtt_down():
        stand.dut.pulses(channel=1, count=2, gap=3.0)

        first = stand.wait_session(timeout=180, mode=ALARM_MODE)
        first.assert_alarm(flow1=1)
        first.assert_confirm(mask=CONFIRM_MQTT, mqtt=SEND_NO_CONNECTION, confirmed=0)

        second = stand.wait_session(timeout=600, mode=ALARM_MODE)
        second.assert_confirm(confirmed=0)

    # Брокер вернулся: ближайший доклад подтверждается и повторы прекращаются
    third = stand.wait_session(timeout=600, mode=ALARM_MODE)
    third.assert_confirm(mqtt=SEND_OK, confirmed=1)


def test_F3_disabled_receiver_drops_out(stand: Stand, quiet: None) -> None:
    """
    Получатель отмечен обязательным, но выключен целиком.

    Требовать от него доставки нельзя: квитанции не будет никогда, и каждая
    тревога стоила бы полного бюджета внеплановых сеансов. Отличие от F2 - одно
    число: mqtt=0 (пропущен), а не 3 (нет связи).
    """
    arm_alarms(stand, confirm_mqtt=1, mqtt_on=0)
    stand.reset_observers()

    stand.dut.pulses(channel=1, count=2, gap=3.0)

    session = stand.wait_session(timeout=120, mode=ALARM_MODE)
    session.assert_confirm(mask=CONFIRM_MQTT, mqtt=SEND_SKIPPED, any=1, confirmed=1)
    stand.expect_no_session(timeout=900, mode=ALARM_MODE)

    stand.setup(mqtt_on=1)


@pytest.mark.slow
def test_F4_alarm_session_budget(stand: Stand, quiet: None, slow_clock: None) -> None:
    """
    Бюджет внеплановых сеансов: не больше пяти на период пробуждения.

    Период на время теста должен быть длинным. При коротком плановое пробуждение
    придёт раньше, чем кончится бюджет: attiny зачтёт его как внеплановое, а
    следом обнулит счёт - и тест насчитает сеансов вдвое больше.

    Окно наблюдения 50 минут, а не полчаса: пауза в пять минут отсчитывается от
    конца сеанса, а сеанс без сети длится до двух минут.
    """
    arm_alarms(stand, period_min=120)
    stand.reset_observers()

    sessions = []
    with stand.net.internet_down():
        stand.dut.pulses(channel=1, count=2, gap=3.0)

        deadline = time.time() + 50 * 60
        while time.time() < deadline:
            session = stand.log.wait_session(timeout=600, mode=ALARM_MODE)
            if session is None:
                break
            sessions.append(session)

    assert len(sessions) == 5, (
        f'ожидали пять внеплановых сеансов, получили {len(sessions)}')

    for session in sessions:
        assert not session.wifi_connected, 'сеть должна была быть недоступна'

    stand.expect_no_session(timeout=15 * 60, mode=ALARM_MODE)
