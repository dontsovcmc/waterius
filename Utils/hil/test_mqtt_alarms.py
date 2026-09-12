"""
MQTT и тревоги - блок I ручного плана, часть про пороги и режимы.

Проверяется: автодискавери объявляет тревожные сущности, команда из Home
Assistant доезжает до ОЗУ attiny, порог пересчитывается в тики по весу
импульса, маска квитанции меняется извне.

Группе нужны attiny 41 и ЕСП 2.0.47: ниже в attiny нет `alarm_bits`, а в
посылке - `vac`, `af1` и `ackm`. Требование стоит на модуле, поэтому на
младшей прошивке пропускается эта группа, а не весь блок MQTT.

Пороги ставит фикстура `armed`: без них в ОЗУ attiny любой тест группы
зеленеет на выключенных тревогах.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .logwatch import Session
    from .stand import Stand

pytestmark = [
    pytest.mark.stand,
    pytest.mark.mqtt,
    pytest.mark.requires(attiny=41, esp='2.0.47'),
    pytest.mark.usefixtures('discovery_reset'),
]

CHANNEL = 1
NAMUR = 0
LEAKAGE = 5                # тип входа «датчик протечки», core/types.h
FACTOR = 10
FLOW_THRESHOLD = 3600      # л/ч; при весе 10 это 40 тиков по 250 мс

# Сущности, появившиеся в 2.0.47. Без явного списка тест зеленеет на
# прошлогоднем наборе автодискавери.
DISCOVERY_SWITCHES = ('vac', 'sc', 'ackw', 'ackh', 'ackm')
DISCOVERY_NUMBERS = ('af1', 'al1', 'as1')
DISCOVERY_BINARY = ('alarm_flow1', 'alarm_leak1', 'alarm_stop1')


@pytest.fixture
def armed(request: pytest.FixtureRequest, stand: Stand) -> Session:
    """
    Пороги в ОЗУ attiny, автодискавери и retain включены.

    `setup_alarms` требует строку `Alarm config:` - без неё пороги остались в
    настройках ЕСП, а тревогу считает attiny, и тест группы проверял бы
    выключенные тревоги. Отдельные настройки теста - маркером `arm`, чтобы это
    стоило того же одного сеанса: на живом железе он идёт полторы минуты.
    """
    settings = dict(factor=FACTOR, alarm_flow=FLOW_THRESHOLD, ctype=NAMUR,
                    vacation=0, mqtt_auto_discovery=1, mqtt_retain=1)
    marker = request.node.get_closest_marker('arm')
    if marker is not None:
        settings.update(marker.kwargs)          # маркер перебивает умолчание
    return stand.setup_alarms(channel=CHANNEL, **settings)


def test_I1_discovery_alarm_entities(stand: Stand, armed: Session) -> None:
    """Автодискавери содержит сущности тревог этого релиза."""
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics('homeassistant/')
    assert topics, 'автодискавери не опубликовано'

    for name in DISCOVERY_SWITCHES:
        assert any('/switch/' in t and f'{name}/config' in t for t in topics), \
            f'нет переключателя {name}'
    for name in DISCOVERY_NUMBERS:
        assert any('/number/' in t and f'{name}/config' in t for t in topics), \
            f'нет числового поля {name}'
    for name in DISCOVERY_BINARY:
        assert any('/binary_sensor/' in t and f'{name}/config' in t for t in topics), \
            f'нет состояния {name}'


def test_I3_remote_vacation_reaches_attiny(stand: Stand, armed: Session) -> None:
    """
    Команда из Home Assistant доезжает до attiny.

    Четыре утверждения, и последнее - главное. Сохранить настройку в EEPROM мало:
    режим «Я уехал» работает только если подменённый порог уехал в attiny, а это
    видно исключительно по строке Alarm config.
    """
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set('vac', 1, retain=True)
    stand.dut.press_button()

    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.applied.get('vac') == '1', f'команда не применена: {session.applied}'
    assert len(session.payloads) >= 2, 'после применения данные должны уйти повторно'
    assert session.payload is not None
    assert session.payload['vac'] is True
    assert session.alarm_config is not None
    assert session.alarm_config['vacation'] == 1
    assert session.alarm_config[f'interval{CHANNEL}'] == 65535

    stand.setup(vacation=0)


def test_I4_remote_threshold_is_recalculated(stand: Stand, armed: Session) -> None:
    """
    Порог, присланный извне, обязан пересчитаться в тики.

    Проверка «значение сохранилось» слабая: она пройдёт и тогда, когда порог
    лежит в настройках, но в attiny не уехал. 1440 л/ч при весе 10 - это ровно
    100 тиков по 250 мс.
    """
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set(f'af{CHANNEL}', 1440, retain=True)
    stand.dut.press_button()

    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.payload is not None
    assert session.payload[f'af{CHANNEL}'] == 1440
    assert session.alarm_config is not None
    assert session.alarm_config[f'interval{CHANNEL}'] == 100, (
        f'порог не пересчитан: {session.alarm_config}')


@pytest.mark.arm(confirm_waterius=0, confirm_http=0, confirm_mqtt=0,
                 ctype=LEAKAGE)
def test_I5_remote_mask_change(stand: Stand, quiet: None, armed: Session) -> None:
    """
    Маска квитанции меняется извне.

    Правило «выключенный получатель выпадает из условия» живёт в прошивке
    именно ради этого пути: в Home Assistant отправителя можно выключить уже
    после того, как галочка поставлена.

    Новость даёт датчик протечки: тревога нужна любая, а эта поднимается за
    секунду и снимается тогда, когда тест отпустит вход.

    Маска задаётся целиком: в эталоне подняты все три бита, и выставив только
    свой, тест увидел бы mask=7.
    """
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set('ackm', 1, retain=True)
    stand.dut.press_button()
    applied = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    assert applied.payload is not None
    assert applied.payload['ackm'] is True

    stand.reset_observers()
    try:
        stand.dut.wet(channel=CHANNEL, closed=True)
        alarm = stand.wait_session(timeout=180)
        alarm.assert_alarm(wet1=1)
        alarm.assert_confirm(mask=4, confirmed=1)
    finally:
        # Вход опрашивается, только пока его тип - датчик: уйти с замкнутым
        # контактом значит оставить поднятый бит следующему тесту, которому
        # вход вернут в NAMUR и снимать тревогу станет некому.
        stand.dut.wet(channel=CHANNEL, closed=False)

    stand.setup(confirm_mqtt=0)
