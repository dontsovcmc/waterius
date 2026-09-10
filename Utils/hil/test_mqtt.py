"""
MQTT: базовый функционал - блок I ручного плана без тревог.

Проверяется: показания уезжают в брокер, дерево автодискавери называет топики
так же, как их считает стенд, команда из Home Assistant применяется в том же
сеансе, флаг retain выставляется по настройке. Всё это умеет любая прошивка с
MQTT, включая 2.0.44.

Пороги и режимы тревог - в `test_mqtt_alarms.py`: им нужны attiny 41 и
ЕСП 2.0.47, здешним проверкам хватает 2.0.44.

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

pytestmark = [pytest.mark.stand, pytest.mark.mqtt,
              pytest.mark.usefixtures('discovery_reset')]

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

NAMUR = 0
LEAKAGE = 5          # CounterType: датчик протечки
WATER_HOT = 1        # CounterName: то, чем канал 0 настроен по умолчанию
HEAT_GCAL = 4        # CounterName: тепло в гигакалориях
HEAT_KWT = 7         # CounterName: то же тепло, но в киловатт-часах

# core/ha_units.h. Единица - не украшение: по ней Home Assistant считает
# статистику, и перепутанная превращает показания в другие числа
HEAT_UNITS = {HEAT_GCAL: 'Gcal', HEAT_KWT: 'kWh'}


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
    assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30) is not None, (
        f'в брокере нет ничего в {stand.mqtt_root}/, пришло: {stand.mqtt.topics()}')

    # Именно этот топик, а не любой в дереве: при включённом автодискавери
    # показания идут одним объектом в корень
    message = stand.mqtt.last(stand.mqtt_root)
    assert message is not None, (
        'показаний в корневом топике нет, дерево: '
        f'{stand.mqtt.topics(stand.mqtt_root)}')
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


def test_I4_remote_period_min(stand: Stand, discovery_on: None) -> None:
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


@pytest.mark.requires(esp='2.0.47')       # младшие снимают команду без флага retain
def test_I4b_retained_command_is_cleared(stand: Stand,
                                        discovery_on: None) -> None:
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


def test_I8_data_topics_are_not_cleared(stand: Stand, discovery_on: None) -> None:
    """
    Устройство снимает retain только со своих команд.

    Подписка накрывает всё дерево, показания уходят в него же и с retain,
    поэтому в следующем сеансе брокер отдаёт устройству его собственные
    удерживаемые сообщения. Снимать с них retain нельзя: это то, из чего Home
    Assistant берёт значения сразу после перезапуска, а следующего сеанса у
    Ватериуса можно ждать сутки (#422).

    Показания кладём в дерево сами: стенд чистит его перед каждым тестом, а
    устройство не отличает своё прошлогоднее сообщение от чужого - брокер
    отдаёт ему и то, и другое одинаково.
    """
    assert stand.mqtt is not None
    stand.reset_observers()

    topic = f'{stand.mqtt_root}/f1'          # топик показаний, не команда
    stand.mqtt.publish_retained(topic, '10')

    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    left = {m.topic: m.payload for m in stand.mqtt.fetch_retained(stand.mqtt_root)}
    assert left.get(topic) == '10', (
        f'показания стёрты из брокера, осталось: {sorted(left)}')


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


def test_I0b_readings_go_to_separate_topics(stand: Stand,
                                            discovery_off: None) -> None:
    """
    Без автодискавери показания уходят по топику на поле.

    Это второй режим публикации (`ha/publish_data.cpp`), и он не следствие
    первого: там один объект в корень, здесь - значение в топик на каждое поле
    посылки. Тест на одном из них ничего не говорит о другом.
    """
    assert stand.mqtt is not None
    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.payload is not None, 'приёмник не получил посылку'
    topics = stand.mqtt.topics(stand.mqtt_root)
    assert topics, f'в брокере пусто, пришло: {stand.mqtt.topics()}'

    # Целые и строки, без плавающей точки: её текстовое представление у
    # прошивки и у python разное, и тест мигал бы на верных данных
    for name in ('imp0', 'imp1', 'rssi', 'version_esp', 'period_min'):
        message = stand.mqtt.last(f'{stand.mqtt_root}/{name}')
        assert message is not None, f'нет топика {name}, есть: {topics}'
        assert message.payload == str(session.payload[name]), (
            f'{name}: в брокере {message.payload!r}, в посылке '
            f'{session.payload[name]!r}')

    assert stand.mqtt.last(stand.mqtt_root) is None, (
        'одним объектом в корень публикуют только при включённом автодискавери')
    assert not stand.mqtt.topics('homeassistant/'), (
        f'автодискавери выключено, а топики опубликованы: '
        f'{stand.mqtt.topics("homeassistant/")}')


def test_I4c_commands_need_discovery(stand: Stand, discovery_off: None) -> None:
    """
    Без автодискавери команда не доезжает - и это не поломка, а устройство.

    Подписку на `<топик>/#` и обработчик команд прошивка заводит только при
    включённом автодискавери (`senders/sender_mqtt.h`). Тест закрепляет это
    явно: иначе выключенный где-то в соседнем тесте флаг превращается в
    загадочное «команда не применена» и обвиняет прошивку не в том.
    """
    assert stand.mqtt is not None
    assert stand.last_payload is not None
    was = stand.last_payload['period_min']

    stand.reset_observers()
    stand.mqtt.publish_set('period_min', OTHER_PERIOD_MIN, retain=True)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.applied == {}, f'настройки применены: {session.applied}'
    assert session.payload is not None
    assert session.payload['period_min'] == was, 'период поменялся без подписки'

    # Главное утверждение - положительное: команду никто не забрал, она так и
    # лежит в брокере удерживаемой. Отсутствие строк в логе доказывало бы то же
    # самое, но только пока METF не теряет строки кольца
    left = [m.topic for m in stand.mqtt.fetch_retained(stand.mqtt_root)]
    assert stand.mqtt.command_topic('period_min') in left, (
        f'команда исчезла из брокера, а подписки не было: {left}')
    assert 'MQTT: Subscribed to' not in session.text


def test_I2_leak_sensor_publishes_only_its_state(stand: Stand) -> None:
    """
    Датчик протечки - не счётчик: в Home Assistant у него есть тип входа и
    состояние влаги, и больше ничего.

    Показания, вес импульса, серийный номер и пороги для него бессмысленны:
    импульсов он не даёт, его тревога - само состояние линии.
    """
    stand.setup(channel=0, ctype=LEAKAGE, mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    try:
        stand.mqtt.drain()
        stand.reset_observers()
        stand.dut.press_button()
        stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        topics = stand.mqtt.topics('homeassistant/')
        assert topics, 'автодискавери не опубликовано'

        assert config_topic(topics, 'select', 'ctype0') is not None, topics
        assert config_topic(topics, 'binary_sensor', 'alarm_wet0') is not None, topics

        pointless = ('ch0', 'f0', 'serial0', 'af0', 'al0', 'as0',
                     'alarm_flow0', 'alarm_leak0', 'alarm_stop0')
        left = [topic for topic in topics
                for name in pointless if topic.endswith(f'/{name}/config')]
        assert not left, f'у датчика протечки объявлено лишнее: {left}'

        # Соседний вход - обычный счётчик, и его сущности на месте: проверка
        # не про «мало топиков», а про то, что молчит именно этот канал
        assert config_topic(topics, 'sensor', 'ch1') is not None, topics
    finally:
        stand.setup(channel=0, ctype=NAMUR)


@pytest.mark.parametrize('resource', sorted(HEAT_UNITS))
def test_I6_heat_carries_its_own_unit(stand: Stand, resource: int) -> None:
    """
    Тепло бывает двух ресурсов, и единица у них разная: гигакалории и
    киловатт-часы. Берётся она по названию канала, а не по типу входа.
    """
    unit = HEAT_UNITS[resource]
    stand.setup(channel=0, cname=resource, mqtt_auto_discovery=1)
    assert stand.mqtt is not None
    try:
        stand.mqtt.drain()
        stand.reset_observers()
        stand.dut.press_button()
        stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        topics = stand.mqtt.topics('homeassistant/')
        topic = config_topic(topics, 'sensor', 'ch0')
        assert topic is not None, topics

        message = stand.mqtt.last(topic)
        assert message is not None
        entity = message.json()
        assert entity.get('unit_of_meas') == unit, entity
    finally:
        stand.setup(channel=0, cname=WATER_HOT)
