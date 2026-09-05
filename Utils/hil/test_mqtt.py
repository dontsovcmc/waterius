"""
Home Assistant и управление извне - блок I ручного плана.

Команда, присланная в топик, применяется в том же сеансе: подписка выполняется
до отправки данных, поэтому удерживаемое сообщение подхватывается сразу, а
после применения данные уходят повторно. Значит проверять надо не «в следующей
посылке», а именно вторую посылку того же сеанса - иначе тест пройдёт и в
случае, когда применение отложилось на сутки.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

NAMUR = 0
FACTOR = 10

# Сущности, появившиеся в 2.0.47. Без явного списка тест зеленеет на
# прошлогоднем наборе автодискавери.
DISCOVERY_SWITCHES = ('vac', 'sc', 'ackw', 'ackh', 'ackm')
DISCOVERY_NUMBERS = ('af1', 'al1', 'as1')
DISCOVERY_BINARY = ('alarm_flow1', 'alarm_leak1', 'alarm_stop1')


def device_name(stand: Stand) -> str:
    """Имя устройства в топиках берём из настроек, а не угадываем."""
    return stand.cfg.mqtt_topic.split('/')[-1]


def test_I1_discovery_published(stand: Stand) -> None:
    """
    Автодискавери публикуется по кнопке и содержит сущности этого релиза.
    """
    stand.setup(mqtt_auto_discovery=1, channel=1, ctype=NAMUR, factor=FACTOR)
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics('homeassistant/')
    assert topics, 'автодискавери не опубликовано'

    for name in DISCOVERY_SWITCHES:
        assert any(f'/switch/' in t and f'{name}/config' in t for t in topics), \
            f'нет переключателя {name}'
    for name in DISCOVERY_NUMBERS:
        assert any('/number/' in t and f'{name}/config' in t for t in topics), \
            f'нет числового поля {name}'
    for name in DISCOVERY_BINARY:
        assert any('/binary_sensor/' in t and f'{name}/config' in t for t in topics), \
            f'нет состояния {name}'


def test_I3_remote_vacation_reaches_attiny(stand: Stand) -> None:
    """
    Команда из Home Assistant доезжает до attiny.

    Четыре утверждения, и последнее - главное. Сохранить настройку в EEPROM мало:
    режим «Я уехал» работает только если подменённый порог уехал в attiny, а это
    видно исключительно по строке Alarm config.
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=3600, ctype=NAMUR,
                       vacation=0, mqtt_auto_discovery=1, mqtt_retain=1)
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set('vac', 1, device=device_name(stand), retain=True)
    stand.dut.press_button()

    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.applied.get('vac') == '1', f'команда не применена: {session.applied}'
    assert len(session.payloads) >= 2, 'после применения данные должны уйти повторно'
    assert session.payload['vac'] == 1
    assert session.alarm_config is not None
    assert session.alarm_config['vacation'] == 1
    assert session.alarm_config['interval1'] == 65535

    stand.setup(vacation=0)


def test_I4_remote_threshold_is_recalculated(stand: Stand) -> None:
    """
    Порог, присланный извне, обязан пересчитаться в тики.

    Проверка «значение сохранилось» слабая: она пройдёт и тогда, когда порог
    лежит в настройках, но в attiny не уехал. 1440 л/ч при весе 10 - это ровно
    100 тиков по 250 мс.
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=3600, ctype=NAMUR,
                       vacation=0, mqtt_auto_discovery=1, mqtt_retain=1)
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set('af1', 1440, device=device_name(stand), retain=True)
    stand.dut.press_button()

    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.payload['af1'] == 1440
    assert session.alarm_config['interval1'] == 100, (
        f'порог не пересчитан: {session.alarm_config}')


def test_I7_retain_flag(stand: Stand) -> None:
    """
    Флаг retain у публикаций.

    Перезапускать брокер незачем: сохранятся ли удерживаемые сообщения, зависит
    от его настроек, а не от прошивки. Проверяем сам флаг в пришедшем сообщении.
    """
    stand.setup(mqtt_retain=1, mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert 'MQTT: Retain: 1' in session.text
    message = stand.mqtt.wait_topic(device_name(stand), timeout=30)
    assert message is not None, 'устройство ничего не опубликовало'
    assert message.retain, 'сообщение пришло без флага retain'


def test_I5_remote_mask_change(stand: Stand, quiet: None) -> None:
    """
    Маска квитанции меняется извне.

    Правило «выключенный получатель выпадает из условия» живёт в прошивке
    именно ради этого пути: в Home Assistant отправителя можно выключить уже
    после того, как галочка поставлена.
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=3600, ctype=NAMUR,
                       vacation=0, confirm_mqtt=0)
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set('ackm', 1, device=device_name(stand), retain=True)
    stand.dut.press_button()
    applied = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    assert applied.payload['ackm'] == 1

    stand.reset_observers()
    stand.dut.pulses(channel=1, count=2, gap=3.0)
    alarm = stand.wait_session(timeout=180)
    alarm.assert_confirm(mask=4, confirmed=1)

    stand.setup(confirm_mqtt=0)
