"""
MQTT и тревоги - блок I ручного плана, часть про пороги и режимы.

Проверяется: автодискавери объявляет тревожные сущности, команда из Home
Assistant доезжает до ОЗУ attiny.

Остальные команды тревог - порог, маска квитанции, снятие - здесь не
повторяются: команда из брокера и ответ сервера ложатся в один документ и
применяются одним `apply_settings` (`main.cpp`). Их проверяют блоки E и F
ответом приёмника, а доставку команды из брокера - I4.

Группе нужны attiny 41 и ЕСП 2.0.47: ниже в attiny нет `alarm_bits`, а в
посылке - `vac`, `ar1` и `ackm`. Требование стоит на модуле, поэтому на
младшей прошивке пропускается эта группа, а не весь блок MQTT.

Пороги ставит фикстура `armed`: без них в ОЗУ attiny любой тест группы
зеленеет на выключенных тревогах.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .constants import BASE_FACTOR, COLD, NAMUR, VOL_LITRES
from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .logwatch import Session
    from .stand import Stand

pytestmark = [
    pytest.mark.stand,
    pytest.mark.mqtt,
    pytest.mark.requires(attiny=41, esp='2.0.47'),
]


# Сущности, появившиеся в 2.0.47. Без явного списка тест зеленеет на
# прошлогоднем наборе автодискавери.
DISCOVERY_SWITCHES = ('vac', 'sc', 'ackw', 'ackh', 'ackm')
DISCOVERY_NUMBERS = ('av1', 'ar1', 'ah1', 'as1')
DISCOVERY_BINARY = ('alarm_flow1', 'alarm_leak1', 'alarm_stop1')
DISCOVERY_BUTTONS = ('arst',)


@pytest.fixture
def armed(stand: Stand) -> Session:
    """
    Пороги в ОЗУ attiny, автодискавери и retain включены.

    `setup_alarms` требует строку `Alarm config:` - без неё пороги остались в
    настройках ЕСП, а тревогу считает attiny, и тест группы проверял бы
    выключенные тревоги.
    """
    return stand.setup_alarms(channel=COLD, factor=BASE_FACTOR, alarm_vol=VOL_LITRES,
                              ctype=NAMUR, vacation=0, mqtt_auto_discovery=1,
                              mqtt_retain=1)


def test_I11_discovery_alarm_entities(stand: Stand, armed: Session) -> None:
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
    # У кнопки нет состояния, и схема HA отвергает лишние ключи целиком: с
    # stat_t в payload сущность не появляется вовсе, молча
    for name in DISCOVERY_BUTTONS:
        assert any('/button/' in t and f'{name}/config' in t for t in topics), \
            f'нет кнопки {name}'


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
    assert session.alarm_config[f'vol{COLD}'] == 1
