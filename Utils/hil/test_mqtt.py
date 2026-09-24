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
from collections.abc import Iterator
from typing import TYPE_CHECKING, Any

import pytest

from .constants import (
    BRAND_NAME,
    ELECTRONIC,
    HEAT_GCAL,
    HEAT_UNITS,
    INPUT_OFF,
    LEAKAGE,
    PLANNED_PERIOD_MIN,
    PLANNED_WAIT_S,
)
from .logwatch import MANUAL_TRANSMIT_MODE, SEND_NO_CONNECTION, SEND_OK, TRANSMIT_MODE

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand  # а сбор тестов должен работать без них

pytestmark = [pytest.mark.stand, pytest.mark.mqtt]

# Автодискавери: при нём показания уходят одним объектом в корень, и только при
# нём прошивка подписывается на команды (`senders/sender_mqtt.h`)
DISCOVERY = pytest.mark.needs(mqtt_auto_discovery=1)
NO_DISCOVERY = pytest.mark.needs(mqtt_auto_discovery=0)

# Дерево автообнаружения у брокера: конфиги там удерживаемые, и стенд их
# между тестами не чистит - чистит сам тест, которому важно, что лежит
DISCOVERY_ROOT = 'homeassistant'

# Сущности входа 0, которых у датчика протечки быть не должно
COUNTER_ONLY = ('ch0', 'f0', 'serial0', 'cname0', 'av0', 'ar0', 'ah0', 'as0',
                'alarm_flow0', 'alarm_leak0', 'alarm_stop0')

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

# Период на время теста: любое значение, отличное от общего, - лишь бы
# отличалось. Обратно его вернут требования следующего теста.
OTHER_PERIOD_MIN = 90


def config_topic(topics: list[str], entity_type: str, entity_id: str) -> str | None:
    """Топик автодискавери сущности: `homeassistant/<тип>/<устройство>/<имя>/config`."""
    tail = f'/{entity_id}/config'
    return next((t for t in topics
                 if t.startswith(f'{DISCOVERY_ROOT}/{entity_type}/') and t.endswith(tail)),
                None)


def configs_named(topics: list[str], names: tuple[str, ...]) -> list[str]:
    """Топики конфигов сущностей с такими именами, любого типа."""
    return [topic for topic in topics for name in names if topic.endswith(f'/{name}/config')]


@DISCOVERY
def test_I0_readings_reach_broker(stand: Stand) -> None:
    """
    Показания уезжают в брокер, и это ровно та же посылка, что ушла на сервер.

    Проверка на совпадение, а не на «поля похожи»: `send_data` формирует JSON
    один раз и отдаёт его всем получателям (`senders/send_data.cpp`), поэтому
    любое расхождение означает, что путь MQTT собирает данные сам по себе.
    """
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


@DISCOVERY
def test_I1_discovery_base_entities(stand: Stand) -> None:
    """
    Автодискавери публикуется по кнопке и описывает устройство целиком.

    Отдельно - топик команды: стенд шлёт настройки в `<топик>/<сущность>/set`,
    и это утверждение обязано опираться на то, что объявила сама прошивка.
    Иначе тесты команд проверяли бы путь, которым Home Assistant не ходит:
    подписка у прошивки на `<топик>/#`, и лишний сегмент она бы проглотила.
    """
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics(f'{DISCOVERY_ROOT}/')
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


@DISCOVERY
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
    assert len(session.payloads) >= 2, (
        'после применения данные должны уйти повторно' + session.air_note)
    assert session.payload is not None
    assert session.payload['period_min'] == OTHER_PERIOD_MIN


@pytest.mark.requires(esp='2.0.47')       # младшие снимают команду без флага retain
@DISCOVERY
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


# Поздняя команда: любое значение, отличное от эталона - в BASELINE av1 ноль
LATE_VALUE = 50


@pytest.mark.requires(esp='2.0.50')
@DISCOVERY
def test_I4d_late_command_is_not_lost(stand: Stand) -> None:
    """
    Команда, пришедшая после применения настроек, не теряется.

    Применение в сеансе одно, а сокет брокера качается и после него - при
    повторной отправке данных и при отключении. До 2.0.50 команда, попавшая в
    это окно, разбиралась, стиралась у брокера как удерживаемая и не
    применялась: в Home Assistant переключатель показывал успех, а устройство
    просыпалось с прежней настройкой, и повторить было нечем.

    Момент ловим по логу: строка `Apply setting:` означает, что первое
    применение позади. Ранние настройки шлёт приёмник, а не брокер, - так
    порядок задан жёстко, без гонки двух источников.
    """
    assert stand.mqtt is not None

    stand.reset_observers()
    stand.receiver.reply_settings({'period_min': OTHER_PERIOD_MIN})
    stand.dut.press_button()

    applied = stand.log.wait_line('Apply setting:', timeout=180, poll_interval=0.05)
    assert applied is not None, 'первое применение не состоялось - проверять нечего'

    stand.mqtt.publish_set('av1', LATE_VALUE)

    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    assert session.applied.get('av1') == str(LATE_VALUE), (
        f'поздняя команда потеряна: {session.applied}\n{session.text}')

    # Снята она должна быть только после применения - иначе потеря была бы
    # безвозвратной: копии у брокера больше нет
    left = [m.topic for m in stand.mqtt.fetch_retained(stand.mqtt_root)]
    assert stand.mqtt.command_topic('av1') not in left, (
        f'применённая команда осталась удерживаемой: {left}')


@DISCOVERY
def test_I7_retain_flag(stand: Stand) -> None:
    """
    Флаг retain у публикаций.

    Перезапускать брокер незачем: сохранятся ли удерживаемые сообщения, зависит
    от его настроек, а не от прошивки. Смотрим сам флаг - глазами нового
    подписчика, потому что в живой доставке он нулевой у любого брокера
    (MQTT 3.1.1, 3.3.1.3).

    Единица retain - общее требование стенда. На прошивках до 2.0.47 настройка
    не сохраняется вовсе, но там она и так единица (config.cpp, init_config).
    """
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


@DISCOVERY
def test_I8_data_topics_are_not_cleared(stand: Stand) -> None:
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


@DISCOVERY
def test_I1b_discovery_json_is_valid(stand: Stand) -> None:
    """
    Каждый объявленный топик автодискавери - разбираемый JSON с обязательными
    полями. Обрезанная публикация (а данные уходят кусками, `publish_chunked`)
    иначе видна только в Home Assistant, куда стенд не заглядывает.
    """
    assert stand.mqtt is not None
    stand.mqtt.drain()

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics(f'{DISCOVERY_ROOT}/')
    assert topics, 'автодискавери не опубликовано'

    for topic in topics:
        message = stand.mqtt.last(topic)
        assert message is not None
        try:
            entity = json.loads(message.payload)
        except ValueError as error:
            raise AssertionError(
                f'{topic}: не JSON ({error}): {message.payload}') from error
        # Кнопка состояния не имеет, она шлёт команду: у неё обязателен cmd_t
        # (selftest/test_hatemplates.py, test_button_has_no_state)
        component = topic.split('/')[1]
        if component == 'button':
            assert entity.get('cmd_t'), f'{topic}: нет топика команды'
        else:
            assert entity.get('stat_t'), f'{topic}: нет топика состояния'
        assert entity.get('uniq_id'), f'{topic}: нет уникального идентификатора'


def missing(name: str, session: Any, topics: list[str]) -> str:
    """
    Отказ про отсутствующий топик делит вину по логу устройства.

    Прошивка печатает строку на каждую публикацию (`ha/publish.cpp`), поэтому
    видно сразу: топика нет, потому что устройство его не публиковало, или
    потому что сообщение не доехало до брокера стенда.
    """
    said = [line for line in session.text.splitlines() if f'/{name}' in line]
    return (f'нет топика {name}. Устройство про него говорит: '
            f'{said or "ни слова"}. Доехало {len(topics)} топиков: {topics}')


@NO_DISCOVERY
def test_I0b_readings_go_to_separate_topics(stand: Stand) -> None:
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
        assert message is not None, missing(name, session, topics)
        assert message.payload == str(session.payload[name]), (
            f'{name}: в брокере {message.payload!r}, в посылке '
            f'{session.payload[name]!r}')

    # Флаги - словами, как в посылке (#419): топик читают как есть, и 1 в одном
    # месте при true в другом ломает любую автоматизацию
    for name, value in session.payload.items():
        if type(value) is not bool:
            continue
        message = stand.mqtt.last(f'{stand.mqtt_root}/{name}')
        assert message is not None, missing(name, session, topics)
        assert message.payload == ('true' if value else 'false'), (
            f'{name}: в брокере {message.payload!r}, в посылке {value!r}')

    assert stand.mqtt.last(stand.mqtt_root) is None, (
        'одним объектом в корень публикуют только при включённом автодискавери')
    assert not stand.mqtt.topics(f'{DISCOVERY_ROOT}/'), (
        f'автодискавери выключено, а топики опубликованы: '
        f'{stand.mqtt.topics(DISCOVERY_ROOT + "/")}')


@NO_DISCOVERY
def test_I4c_commands_need_discovery(stand: Stand) -> None:
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


@pytest.mark.needs(ctype0=LEAKAGE, mqtt_auto_discovery=1)
def test_I2_leak_sensor_publishes_only_its_state(stand: Stand) -> None:
    """
    Датчик протечки - не счётчик: в Home Assistant у него есть тип входа и
    состояние влаги, и больше ничего.

    Показания, вес импульса, серийный номер и пороги для него бессмысленны:
    импульсов он не даёт, его тревога - само состояние линии.
    """
    assert stand.mqtt is not None
    stand.mqtt.drain()
    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics(f'{DISCOVERY_ROOT}/')
    assert topics, 'автодискавери не опубликовано'

    assert config_topic(topics, 'select', 'ctype0') is not None, topics
    assert config_topic(topics, 'binary_sensor', 'alarm_wet0') is not None, topics

    left = configs_named(topics, COUNTER_ONLY)
    assert not left, f'у датчика протечки объявлено лишнее: {left}'

    # Соседний вход - обычный счётчик, и его сущности на месте: проверка
    # не про «мало топиков», а про то, что молчит именно этот канал
    assert config_topic(topics, 'sensor', 'ch1') is not None, topics


@pytest.mark.parametrize('resource', sorted(HEAT_UNITS))
@DISCOVERY
def test_I6_heat_carries_its_own_unit(stand: Stand, resource: int) -> None:
    """
    Тепло бывает двух ресурсов, и единица у них разная: гигакалории и
    киловатт-часы. Берётся она по названию канала, а не по типу входа.
    """
    unit = HEAT_UNITS[resource]
    stand.setup(channel=0, cname=resource)
    assert stand.mqtt is not None
    stand.mqtt.drain()
    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = stand.mqtt.topics(f'{DISCOVERY_ROOT}/')
    topic = config_topic(topics, 'sensor', 'ch0')
    assert topic is not None, topics

    message = stand.mqtt.last(topic)
    assert message is not None
    entity = message.json()
    assert entity.get('unit_of_meas') == unit, entity


@DISCOVERY
def test_I9_both_input_types_in_one_session(stand: Stand) -> None:
    """
    Два типа входа одним сеансом применяются оба (#360).

    Тип уходит в attiny парой (`setCountersType(t0, t1)`), и команда для
    одного входа берёт тип соседа из живой копии `runtime_data`. Не обнови
    первая команда эту копию - вторая вернула бы соседу прежний тип, и
    изменился бы только один вход. Так issue и описан: ch0 не меняется.

    Значения разные и оба не NAMUR: перепутанные каналы тоже видны.
    """
    assert stand.mqtt is not None
    stand.reset_observers()
    stand.mqtt.publish_set('ctype0', ELECTRONIC, retain=True)
    stand.mqtt.publish_set('ctype1', INPUT_OFF, retain=True)
    try:
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        assert session.applied.get('ctype0') == str(ELECTRONIC), session.applied
        assert session.applied.get('ctype1') == str(INPUT_OFF), session.applied
        assert session.payload is not None
        assert session.payload['ctype0'] == ELECTRONIC, (
            f"вход 0 не сменил тип: {session.payload['ctype0']}")
        assert session.payload['ctype1'] == INPUT_OFF, (
            f"вход 1 не сменил тип: {session.payload['ctype1']}")
    finally:
        # Устройство снимает свои команды само (I4b), но упавший тест иначе
        # оставил бы их применяться в каждом следующем сеансе
        for name in ('ctype0', 'ctype1'):
            stand.mqtt.clear_retained(stand.mqtt.command_topic(name))


@pytest.mark.requires(esp='2.0.47')       # снятие своей команды без эха (#421)
@DISCOVERY
def test_I14_invalid_command_is_dropped(stand: Stand) -> None:
    """
    Негодная команда не применяется, но из брокера снимается.

    Иначе она лежала бы удерживаемой и прилетала каждое пробуждение - #409,
    только с мусором. Неизвестное имя - та же история: сообщение уже в топике
    `/set`, и прошивка обязана его освободить.
    """
    assert stand.mqtt is not None
    assert stand.last_payload is not None
    was = stand.last_payload['period_min']
    names = ('period_min', 'nosuch')

    stand.reset_observers()
    stand.mqtt.publish_set('period_min', 'abc', retain=True)
    stand.mqtt.publish_set('nosuch', 1, retain=True)
    try:
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        assert session.applied.get('period_min') == 'abc', session.applied
        assert session.payload is not None
        assert session.payload['period_min'] == was, 'негодный период применён'

        left = [m.topic for m in stand.mqtt.fetch_retained(stand.mqtt_root)]
        stuck = [name for name in names if stand.mqtt.command_topic(name) in left]
        assert not stuck, f'команды остались удерживаемыми: {stuck}'
    finally:
        for name in names:
            stand.mqtt.clear_retained(stand.mqtt.command_topic(name))


def discovery_entities(stand: Stand) -> dict[tuple[str, str], dict]:
    """
    Автообнаружение у брокера: (тип, имя сущности) -> конфиг.

    Источник - удерживаемые конфиги, а не услышанное в эфире: Home Assistant
    читает их у брокера при своём старте, и судить о сущностях надо по тому же
    месту. Прошивка публикует заново только при смене отпечатка
    (`core/discovery.h`), поэтому сеанс, в котором ничего не изменилось, живой
    публикации не даёт - а сущности у брокера при этом на месте и верны.
    """
    entities = {}
    for topic, payload in retained_configs(stand).items():
        parts = topic.split('/')          # homeassistant/<тип>/<устройство>/<имя>/config
        if len(parts) == 5:
            entities[(parts[1], parts[3])] = json.loads(payload)
    return entities


@DISCOVERY
def test_I15_templates_render_the_payload(stand: Stand) -> None:
    """
    Каждая сущность автодискавери, отрендеренная по посылке, показывает своё.

    I1b проверяет, что конфиг - JSON; этот - что из него выйдет в Home
    Assistant: шаблон читает поле своего канала (#288, #319), флаг приведён к
    1/0 своего `stat_on` (#419), атрибуты собираются в JSON (PR #348), поле
    есть в посылке. Рендер - `hatemplates.py`.
    """
    from .hatemplates import problems

    assert stand.mqtt is not None
    stand.mqtt.drain()
    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    root = stand.mqtt.last(stand.mqtt_root)
    assert root is not None, 'показаний в корне нет - рендерить не по чему'
    payload = root.json()

    entities = discovery_entities(stand)
    assert entities, 'автодискавери не опубликовано'
    found = [problem for (kind, name), entity in sorted(entities.items())
             for problem in problems(kind, name, entity, payload)]
    assert not found, '\n'.join(found)


@pytest.mark.needs(ctype0=INPUT_OFF, mqtt_auto_discovery=1)
def test_I15b_disabled_input_publishes_only_its_type(stand: Stand) -> None:
    """
    Выключенный вход объявляет только свой тип, соседний - всё (#319).

    #319: при выключенной горячей воде Home Assistant получал её сенсоры, а
    холодной не видел. Тип входа публикуется и у выключенного
    (`ha/publish_discovery.cpp`): иначе включить вход из HA было бы нечем.
    """
    assert stand.mqtt is not None
    stand.mqtt.drain()
    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    names = {name for _, name in discovery_entities(stand)}
    assert names, 'автодискавери не опубликовано'
    assert {name for name in names if name.endswith('0')} == {'ctype0'}, sorted(names)
    assert 'ch1' in names, f'у включённого входа нет показаний: {sorted(names)}'


# Пароль брокера длиной 64 символа: столько генерирует Home Assistant (#301)
AUTH_USER = 'waterius'
AUTH_PASSWORD = 'Aa0' * 21 + 'Z'


@pytest.fixture
def auth_broker(cfg: Any) -> Iterator[Any]:
    """Брокер с паролями на соседнем порту: основной пускает любого."""
    from .broker import MqttBroker
    server = MqttBroker(cfg.broker_port + 1, cfg.broker_host,
                        users={AUTH_USER: AUTH_PASSWORD})
    server.start()
    try:
        yield server
    finally:
        server.stop()


@pytest.mark.requires(esp='2.0.47')       # статус брокера - строкой Alarm confirm
def test_I16_long_mqtt_password(stand: Stand, cfg: Any, auth_broker: Any) -> None:
    """
    Пароль брокера в 64 символа доходит до брокера целиком (#301).

    Брокер отдельный, с паролями: основной пускает любого, и обрезанный
    пароль на нём прошёл бы незамеченным. Неверный пароль - зеркало:
    подключение отвергнуто, а не принято как-нибудь.
    """
    try:
        stand.setup(mqtt_port=auth_broker.port, mqtt_login=AUTH_USER,
                    mqtt_password=AUTH_PASSWORD)
        stand.reset_observers()
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert 'MQTT: Connected.' in session.text, session.text
        session.assert_confirm(mqtt=SEND_OK)

        stand.setup(mqtt_password=AUTH_PASSWORD[:-1] + 'x')
        stand.reset_observers()
        stand.dut.press_button()
        refused = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert 'MQTT: Connect failed with state' in refused.text, refused.text
        refused.assert_confirm(mqtt=SEND_NO_CONNECTION)
    finally:
        stand.setup(mqtt_port=cfg.broker_port, mqtt_login='', mqtt_password='')


def retained_configs(stand: Stand) -> dict[str, str]:
    """Удерживаемые конфиги автообнаружения: топик -> тело. Пустые брокер не хранит."""
    assert stand.mqtt is not None
    # Удерживаемое локальный брокер отдаёт за доли секунды: пяти секунд по умолчанию не нужно
    return {m.topic: m.payload for m in stand.mqtt.fetch_retained(DISCOVERY_ROOT, timeout=1.0)
            if m.payload}


@pytest.mark.requires(esp='2.0.51')
@DISCOVERY
def test_I17_discovery_is_retained_without_retain_flag(stand: Stand) -> None:
    """
    Конфиги автообнаружения удерживаются брокером и при выключенном retain.

    Home Assistant читает конфиги у брокера при своём старте, а спящее
    устройство переслать их по его просьбе не может. Без retain после
    перезапуска HA сущности пропадали до нажатия кнопки. Показания при этом
    по-прежнему публикуются по настройке - это проверяет зеркальный I7b.
    """
    stand.setup(mqtt_retain=0)
    assert stand.mqtt is not None
    stand.mqtt.clear_retained_tree(DISCOVERY_ROOT)

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    topics = list(retained_configs(stand))
    for entity_type, entity_id in BASE_ENTITIES:
        assert config_topic(topics, entity_type, entity_id) is not None, (
            f'конфиг {entity_type}/{entity_id} не удержан брокером, удержаны: {topics}')

    left = [m.topic for m in stand.mqtt.fetch_retained(stand.mqtt_root, timeout=1.0)]
    assert stand.mqtt_root not in left, (
        f'показания удержаны при mqtt_retain=0: {left}')



@pytest.mark.requires(esp='2.0.51')
@DISCOVERY
def test_I18_changed_input_type_removes_stale_entities(stand: Stand) -> None:
    """
    Вход стал датчиком протечки - сущности счётчика удалены из Home Assistant.

    Удалить сущность можно только пустым конфигом с retain (спецификация HA
    MQTT Discovery). Без него показания, вес и пороги входа оставались у
    брокера и в HA навсегда, хотя I2 и видел, что в свежем автообнаружении их
    нет: он смотрит только на то, что опубликовано, а не на то, что осталось.
    """
    assert stand.mqtt is not None
    stand.mqtt.clear_retained_tree(DISCOVERY_ROOT)
    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    before = list(retained_configs(stand))
    assert config_topic(before, 'sensor', 'ch0') is not None, (
        f'у счётчика нет показаний - удалять нечего: {before}')

    stand.setup(channel=0, ctype=LEAKAGE)

    after = list(retained_configs(stand))
    assert config_topic(after, 'select', 'ctype0') is not None, after
    assert config_topic(after, 'binary_sensor', 'alarm_wet0') is not None, after
    stale = configs_named(after, COUNTER_ONLY)
    assert not stale, f'сущности счётчика остались у брокера: {stale}'
    assert config_topic(after, 'sensor', 'ch1') is not None, (
        f'соседний вход потерял показания: {after}')


# Пороги тревог первых сборок dev 2.0.47 - их заменили av/ar/ah
RETIRED = ('af0', 'af1', 'al0', 'al1')


@pytest.mark.requires(esp='2.0.51')
@DISCOVERY
def test_I21_retired_entities_are_removed(stand: Stand) -> None:
    """
    Сущности af и al удаляются у брокера при публикации автообнаружения.

    Их публиковали сборки dev 2.0.47, а тестировщики dev могли оставить у своего
    брокера удерживаемые конфиги: в HA это пороги, которые больше ничего не
    меняют. Конфиги кладём сами - как лежали бы они после такой сборки.
    """
    assert stand.mqtt is not None
    assert stand.last_payload is not None
    device = f"{BRAND_NAME}-{stand.last_payload['esp_id']}"
    planted = [f'{DISCOVERY_ROOT}/number/{device}/{name}/config' for name in RETIRED]
    for topic in planted:
        stand.mqtt.publish_retained(topic, '{"name": "retired"}')

    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    left = [topic for topic in planted if topic in retained_configs(stand)]
    assert not left, f'устаревшие сущности остались у брокера: {left}'


@pytest.mark.slow
@pytest.mark.requires(esp='2.0.51')
@pytest.mark.needs(mqtt_auto_discovery=1, period_min=PLANNED_PERIOD_MIN)
def test_I19_planned_session_republishes_changed_discovery(stand: Stand) -> None:
    """
    Ресурс входа сменился вне кнопки - автообнаружение переотправлено в том же сеансе.

    Настройку меняет сервер или Home Assistant в плановом сеансе, а
    автообнаружение публиковалось только по кнопке. Сущность оставалась
    прежней: канал, ставший теплом, жил в HA водой в m³ до нажатия кнопки.
    """
    assert stand.last_payload is not None
    assert stand.last_payload['cname0'] != HEAT_GCAL, 'ресурс уже тот - проверять нечего'

    session = stand.setup(channel=0, cname=HEAT_GCAL, wake=False, timeout=PLANNED_WAIT_S)
    assert session.mode == TRANSMIT_MODE, f'сеанс не плановый: mode={session.mode}'

    assert stand.mqtt is not None
    topics = stand.mqtt.topics(f'{DISCOVERY_ROOT}/')
    topic = config_topic(topics, 'sensor', 'ch0')
    assert topic is not None, f'в плановом сеансе автообнаружение не переотправлено: {topics}'
    message = stand.mqtt.last(topic)
    assert message is not None
    assert message.json().get('unit_of_meas') == HEAT_UNITS[HEAT_GCAL], message.payload


@pytest.fixture
def cutting_broker(cfg: Any) -> Iterator[Any]:
    """Брокер, рвущий соединение на первом конфиге автообнаружения."""
    from .broker import CuttingBroker
    server = CuttingBroker(cfg.broker_port + 2, cfg.broker_host)
    server.start()
    try:
        yield server
    finally:
        server.stop()


@pytest.mark.requires(esp='2.0.51')
@DISCOVERY
def test_I20_lost_broker_is_not_reported_as_delivered(stand: Stand, cfg: Any,
                                                      cutting_broker: Any) -> None:
    """
    Брокер оборвал сеанс посреди публикаций - доставка по MQTT не засчитана.

    Статус MQTT решает, подтверждать ли attiny тревогу и сдвигать ли точку
    отсчёта дельты. Прошивка проверяла только, открыт ли сокет перед первой
    публикацией, и засчитывала сеанс, в котором показания до брокера не дошли.
    """
    try:
        stand.setup(mqtt_port=cutting_broker.port)
        stand.reset_observers()
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        assert 'MQTT: Connected.' in session.text, session.text
        assert cutting_broker.cut.is_set(), 'брокер не порвал соединение - проверять нечего'
        session.assert_confirm(mqtt=SEND_NO_CONNECTION)
    finally:
        stand.setup(mqtt_port=cfg.broker_port)
