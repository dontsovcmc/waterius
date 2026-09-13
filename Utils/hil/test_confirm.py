"""
Кому доклад о тревоге обязан доехать - блок F ручного плана.

Все четыре теста читают одну строку лога:

    Alarm confirm: mask=4 waterius=1 http=0 mqtt=3 any=1 -> 0

Утверждается она целиком, а не только итог: `-> 1` печатается при любой маске
и любом удачном сеансе, то есть по одному итогу тест зелёный и при полностью
выключенных тревогах. F2 от F3 отличает одно число: mqtt=3 - брокер
недоступен, mqtt=0 - получатель выключен и из условия выпадает.

Тревогу поднимает датчик протечки, а не порог расхода. Проверяется здесь
доставка доклада, и источник новости нужен самый управляемый: замыкание входа
поднимает тревогу за секунду и не зависит ни от веса импульса, ни от окна
наблюдения. Сами пороги расхода экспериментальные и здесь ни при чём.

Ни одна тревога не гаснет сама, поэтому снятие - тоже новость, но случается
она только по воле человека. Отпущенный датчик тревогу не снимает; там, где
тесту нужно чистое состояние, он нажимает кнопку.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest

from .logwatch import (ALARM_MODE, SEND_NO_CONNECTION, SEND_OK, SEND_SKIPPED)
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

# Вся группа поднимает тревогу, чтобы получить квитанцию: до attiny 41 её нет.
pytestmark = [pytest.mark.stand, pytest.mark.mqtt,
              pytest.mark.requires(attiny=41)]

LEAKAGE = 5             # тип входа, core/types.h
SENSOR = 0              # канал с датчиком протечки

CONFIRM_MQTT = 4        # AlarmConfirm, core/types.h

# Сколько наблюдаем тишину, чтобы утверждать «больше не будит». Пауза после
# сеанса по тревоге - ALARM_HOLD_MIN, пять минут (Attiny85/src/alarm.h), и
# раньше неё внеплановый сеанс невозможен физически. Берём её плюс сеанс и
# запас; ждать дольше - платить временем за уже доказанное.
SILENCE_S = 420.0


def arm_sensor(stand: Stand, **extra: int) -> None:
    """
    Общее предусловие: вход - датчик протечки, контакт разомкнут.

    Маску квитанции тест обязан задать целиком. В эталоне подняты все три бита
    (BASELINE: ackw, ackh, ackm), и тест, выставивший только свой, получил бы
    mask=7 вместо ожидаемого.

    Контакт отпускается здесь же: пока он замкнут, `set_wet` поднимает тревогу
    заново на каждом тике, и снять её не сможет ни кнопка, ни маска. А вход
    опрашивается, лишь пока его тип - датчик, так что после возврата в NAMUR
    отпускать его будет поздно.
    """
    stand.dut.wet(channel=SENSOR, closed=False)
    stand.setup(channel=SENSOR, ctype=LEAKAGE, **extra)


def test_F1_any_receiver_is_enough(stand: Stand, quiet: None) -> None:
    """
    Маска пустая: хватает любого получателя, повторных сеансов нет.

    Заодно это и проверка F6: подтверждённая тревога больше не будит устройство
    вовсе. Раньше потолок задавал бюджет внеплановых сеансов, теперь - сама
    модель: тревога поднимается один раз и молчит до снятия.
    """
    arm_sensor(stand, confirm_waterius=0, confirm_http=0, confirm_mqtt=0)
    stand.reset_observers()

    try:
        stand.dut.wet(channel=SENSOR, closed=True)

        session = stand.wait_session(timeout=120, mode=ALARM_MODE)
        session.assert_alarm(wet0=1)
        session.assert_confirm(mask=0, any=1, confirmed=1)
    finally:
        stand.dut.wet(channel=SENSOR, closed=False)

    # Окно чуть больше ALARM_HOLD_MIN: раньше attiny не проснулся бы в любом
    # случае, а дольше держать нечего - плановые сеансы идут своим чередом и
    # под mode не попадают.
    stand.expect_no_session(timeout=SILENCE_S, mode=ALARM_MODE)


@pytest.mark.slow
def test_F2_required_receiver_unreachable(stand: Stand, quiet: None) -> None:
    """
    MQTT отмечен обязательным и недоступен: квитанции нет, attiny будит ЕСП
    снова. Брокер при этом остаётся включённым в настройках - иначе получится
    совсем другой сценарий, F3.
    """
    arm_sensor(stand, confirm_waterius=0, confirm_http=0, confirm_mqtt=1)
    stand.reset_observers()

    try:
        with stand.net.mqtt_down():
            stand.dut.wet(channel=SENSOR, closed=True)

            first = stand.wait_session(timeout=180, mode=ALARM_MODE)
            first.assert_alarm(wet0=1)
            first.assert_confirm(mask=CONFIRM_MQTT, mqtt=SEND_NO_CONNECTION,
                                 confirmed=0)

            second = stand.wait_session(timeout=600, mode=ALARM_MODE)
            second.assert_confirm(confirmed=0)

        # Брокер вернулся: ближайший доклад подтверждается и повторы прекращаются
        third = stand.wait_session(timeout=600, mode=ALARM_MODE)
        third.assert_confirm(mqtt=SEND_OK, confirmed=1)
    finally:
        stand.dut.wet(channel=SENSOR, closed=False)


def test_F3_disabled_receiver_drops_out(stand: Stand, quiet: None) -> None:
    """
    Получатель отмечен обязательным, но выключен целиком.

    Требовать от него доставки нельзя: квитанции не будет никогда, и каждая
    тревога стоила бы полного бюджета внеплановых сеансов. Отличие от F2 - одно
    число: mqtt=0 (пропущен), а не 3 (нет связи).
    """
    arm_sensor(stand, confirm_waterius=0, confirm_http=0, confirm_mqtt=1,
               mqtt_on=0)
    stand.reset_observers()

    try:
        stand.dut.wet(channel=SENSOR, closed=True)

        session = stand.wait_session(timeout=120, mode=ALARM_MODE)
        session.assert_confirm(mask=CONFIRM_MQTT, mqtt=SEND_SKIPPED, any=1, confirmed=1)

        stand.dut.wet(channel=SENSOR, closed=False)
        stand.expect_no_session(timeout=SILENCE_S, mode=ALARM_MODE)
    finally:
        stand.dut.wet(channel=SENSOR, closed=False)
        # Брокер обязан вернуться даже после падения: в BASELINE его нет, и
        # выключенным его унаследует весь блок I. Падать будет он, а
        # разбираться придётся здесь.
        stand.setup(mqtt_on=1)


@pytest.mark.slow
def test_F4_delivery_attempts(stand: Stand, quiet: None) -> None:
    """
    Потолок попыток доставки: не больше ALARM_MAX_TRIES, то есть пяти.

    Бюджета внеплановых сеансов больше нет - считать нечего, тревога поднимается
    один раз. Осталось ограничение на повторы неподтверждённого доклада: без
    сети сеансы только жгут батарею, а состояние всё равно уедет ближайшим
    плановым выходом на связь.

    Период на время теста должен быть длинным: короткий даст плановые сеансы
    внутри окна наблюдения, и счёт собьётся.

    Окно наблюдения 50 минут, а не полчаса: пауза в пять минут отсчитывается от
    конца сеанса, а сеанс без сети длится до двух минут.
    """
    arm_sensor(stand, period_min=120)
    stand.reset_observers()

    sessions = []
    try:
        with stand.net.internet_down():
            # Датчик остаётся замкнутым всё окно: новость одна, и считаем мы
            # попытки её доставить, а не число новостей.
            stand.dut.wet(channel=SENSOR, closed=True)

            deadline = time.time() + 50 * 60
            while time.time() < deadline:
                session = stand.log.wait_session(timeout=600, mode=ALARM_MODE)
                if session is None:
                    break
                sessions.append(session)
    finally:
        stand.dut.wet(channel=SENSOR, closed=False)

    assert len(sessions) == 5, (
        f'ожидали пять попыток доставки, получили {len(sessions)}')

    # Wi-Fi при этом поднимается: правило фильтра режет трафик, а не эфир.
    # Признак неудачи - не отсутствие связи, а несостоявшаяся доставка.
    for session in sessions:
        assert session.confirm is not None, f'нет строки Alarm confirm\n{session.text}'
        assert session.confirm['confirmed'] == 0, (
            f'квитанция не могла состояться: {session.confirm}')

    stand.expect_no_session(timeout=SILENCE_S, mode=ALARM_MODE)
