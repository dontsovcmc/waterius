"""
MQTT: базовый функционал - блок I ручного плана без тревог.

Здесь то, что умеет любая прошивка с MQTT: показания уезжают в брокер, дерево
автодискавери называет топики так же, как их считает стенд, команда из Home
Assistant применяется в том же сеансе. Всё это проверяется и на 2.0.44.

Тревоги вынесены в `test_mqtt_alarms.py`: им нужны attiny 41 и ЕСП 2.0.47, а
держать их вместе значит пропускать весь блок из-за прошивки, которой на
базовую проверку хватает.

Команда применяется в том же сеансе, где получена: подписка выполняется до
отправки данных, поэтому удерживаемое сообщение подхватывается сразу, а после
применения данные уходят повторно. Значит проверять надо не «в следующей
посылке», а именно вторую посылку того же сеанса - иначе тест пройдёт и в
случае, когда применение отложилось на сутки.
"""

from __future__ import annotations

import json
from typing import TYPE_CHECKING

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = [pytest.mark.stand, pytest.mark.mqtt]

# Сущности автодискавери, которые прошивка публикует независимо от тревог
# (`ha/publish_discovery.cpp`). Тип в топике важен не меньше имени: `number`
# отличается от `sensor` наличием топика команды.
BASE_ENTITIES = (
    ('number', 'period_min'),
    ('sensor', 'voltage'),
    ('sensor', 'rssi'),
    ('sensor', 'ip'),
    ('sensor', 'model'),
)

# Период на время теста: любое значение, отличное от базового, - лишь бы
# отличалось. К базовому его вернёт ensure_baseline перед следующим тестом.
OTHER_PERIOD_MIN = 90


def config_topic(topics: list[str], entity_type: str, entity_id: str) -> str | None:
    """Топик автодискавери сущности: `homeassistant/<тип>/<устройство>/<имя>/config`."""
    tail = f'/{entity_id}/config'
    return next((t for t in topics
                 if t.startswith(f'homeassistant/{entity_type}/') and t.endswith(tail)),
                None)


def test_I0_readings_reach_broker(stand: Stand) -> None:
    """
    Показания уезжают в брокер, и это ровно та же посылка, что ушла на сервер.

    Проверка на совпадение, а не на «поля похожи»: `send_data` формирует JSON
    один раз и отдаёт его всем получателям (`senders/send_data.cpp`), поэтому
    любое расхождение означает, что путь MQTT собирает данные сам по себе.
    """
    stand.setup(mqtt_auto_discovery=1)       # показания одним объектом в корень
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert 'MQTT: Connected.' in session.text, f'брокер недоступен\n{session.text}'
    message = stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30)
    assert message is not None, (
        f'в брокере нет ничего в {stand.mqtt_root}/, пришло: {stand.mqtt.topics()}')

    assert message.topic == stand.mqtt_root, (
        'при включённом автодискавери показания идут одним объектом в корневой '
        f'топик, а пришли в {message.topic}')
    assert session.payload is not None, 'приёмник не получил посылку'
    assert message.json() == session.payload, (
        'в брокер и на сервер ушли разные данные')


def test_I1_discovery_base_entities(stand: Stand) -> None:
    """
    Автодискавери публикуется по кнопке и описывает устройство целиком.

    Отдельно - топик команды: стенд шлёт настройки в `<топик>/<сущность>/set`,
    и это утверждение обязано опираться на то, что объявила сама прошивка.
    Иначе тесты команд проверяли бы путь, которым Home Assistant не ходит:
    подписка у прошивки на `<топик>/#`, и лишний сегмент она бы проглотила.
    """
    stand.setup(mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics('homeassistant/')
    assert topics, 'автодискавери не опубликовано'

    for entity_type, entity_id in BASE_ENTITIES:
        assert config_topic(topics, entity_type, entity_id) is not None, (
            f'нет сущности {entity_type}/{entity_id}, есть: {topics}')

    topic = config_topic(topics, 'number', 'period_min')
    assert topic is not None
    message = stand.mqtt.last(topic)
    assert message is not None
    command_topic = message.json().get('cmd_t')
    assert command_topic == stand.mqtt.command_topic('period_min'), (
        f"стенд шлёт команды в {stand.mqtt.command_topic('period_min')}, "
        f'а устройство ждёт их в {command_topic}')


def test_I4_remote_period_min(stand: Stand) -> None:
    """
    Настройка, присланная из Home Assistant, применяется в том же сеансе.

    Период выбран потому, что он есть в любой прошивке с MQTT и виден в
    посылке: проверяем не «сохранилось в EEPROM», а то, что устройство само
    сообщает о себе после применения.
    """
    assert stand.mqtt is not None
    stand.reset_observers()
    stand.mqtt.publish_set('period_min', OTHER_PERIOD_MIN, retain=True)
    stand.dut.press_button()

    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.applied.get('period_min') == str(OTHER_PERIOD_MIN), (
        f'команда не применена: {session.applied}')
    assert len(session.payloads) >= 2, 'после применения данные должны уйти повторно'
    assert session.payload is not None
    assert session.payload['period_min'] == OTHER_PERIOD_MIN


@pytest.mark.requires(esp='2.0.47')       # #409: снятие уходило без флага retain
def test_I4b_retained_command_is_cleared(stand: Stand) -> None:
    """
    Применив удерживаемую команду, устройство обязано стереть её из брокера.

    Иначе она прилетает каждое пробуждение и переустанавливает настройку -
    поменять её из портала станет невозможно. Проверять надо при выключенном
    `mqtt_retain`: до 2.0.47 пустое сообщение уходило с флагом из настроек
    (`ha/publish.cpp`, publish_simple), то есть при нуле команда оставалась
    висеть, а при единице всё выглядело исправным.
    """
    stand.setup(mqtt_retain=0)
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.mqtt.publish_set('period_min', OTHER_PERIOD_MIN, retain=True)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.applied.get('period_min') == str(OTHER_PERIOD_MIN), (
        f'команда не доехала: {session.applied}')

    command_topic = stand.mqtt.command_topic('period_min')
    left = [m.topic for m in stand.mqtt.fetch_retained(stand.mqtt_root)]
    assert command_topic not in left, (
        f'команда осталась в брокере удерживаемой: {left}')

    stand.setup(mqtt_retain=1)


def test_I7_retain_flag(stand: Stand) -> None:
    """
    Флаг retain у публикаций.

    Перезапускать брокер незачем: сохранятся ли удерживаемые сообщения, зависит
    от его настроек, а не от прошивки. Смотрим сам флаг - глазами нового
    подписчика, потому что в живой доставке он нулевой у любого брокера
    (MQTT 3.1.1, 3.3.1.3).
    """
    # На прошивках до 2.0.47 эта настройка не сохраняется вовсе, но там
    # `mqtt_retain` и так единица по умолчанию (config.cpp, init_config)
    stand.setup(mqtt_retain=1, mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert 'MQTT: Retain: 1' in session.text
    assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30) is not None, \
        'устройство ничего не опубликовало'

    retained = stand.mqtt.fetch_retained(stand.mqtt_root)
    assert retained, f'в брокере не осталось ничего в {stand.mqtt_root}/'
    assert all(m.retain for m in retained)
    assert any(m.topic == stand.mqtt_root for m in retained), (
        f'показаний среди удерживаемых нет: {[m.topic for m in retained]}')


@pytest.mark.requires(esp='2.0.47')       # до неё чекбокс retain не сохранялся
def test_I7b_no_retain(stand: Stand) -> None:
    """
    Зеркальный случай: при выключенном retain в брокере ничего не остаётся.

    Без него предыдущий тест доказывает только то, что публикация вообще была:
    брокер держал бы сообщение и при неверном флаге, если бы его выставлял
    кто-то другой.

    Требует 2.0.47: до неё `mqtt_retain` не разбирался в applyCheckBoxParameter
    (`portal/active_point_api.cpp`, коммит cffafb6) - настройка приезжала,
    печаталась строкой `Apply setting:` и молча пропадала.
    """
    stand.setup(mqtt_retain=0, mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert 'MQTT: Retain: 0' in session.text
    assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30) is not None, \
        'устройство ничего не опубликовало'

    left = [m.topic for m in stand.mqtt.fetch_retained(stand.mqtt_root)]
    assert stand.mqtt_root not in left, (
        f'показания остались удерживаемыми при mqtt_retain=0: {left}')

    stand.setup(mqtt_retain=1)


def test_I1b_discovery_json_is_valid(stand: Stand) -> None:
    """
    Каждый объявленный топик автодискавери - разбираемый JSON с обязательными
    полями. Обрезанная публикация (а данные уходят кусками, `publish_chunked`)
    иначе видна только в Home Assistant, куда стенд не заглядывает.
    """
    stand.setup(mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics('homeassistant/')
    assert topics, 'автодискавери не опубликовано'

    for topic in topics:
        message = stand.mqtt.last(topic)
        assert message is not None
        try:
            entity = json.loads(message.payload)
        except ValueError as error:
            raise AssertionError(f'{topic}: не JSON ({error}): {message.payload}')
        assert entity.get('stat_t'), f'{topic}: нет топика состояния'
        assert entity.get('uniq_id'), f'{topic}: нет уникального идентификатора'
