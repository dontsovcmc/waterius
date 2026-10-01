"""
Брокер и подписчик - без железа.

Проверяется то, на чём блок MQTT стоит целиком: брокер поднимается в процессе
тестов, команда уходит в тот же топик, который прошивка объявляет Home
Assistant, а удерживаемое сообщение видно только новому подписчику. Последнее
неочевидно: живая доставка идёт с нулевым флагом retain у любого брокера
(MQTT 3.1.1, 3.3.1.3), поэтому флаг проверяется новым подписчиком.
"""

from __future__ import annotations

import threading
import time
from collections.abc import Iterator
from typing import Any

import pytest

from ..broker import MqttBroker
from . import free_port

if not MqttBroker.available():
    pytest.skip('нет amqtt: pip install -r Utils/hil/requirements.txt',
                allow_module_level=True)

TOPIC = 'waterius/stand'


@pytest.fixture
def broker() -> Iterator[MqttBroker]:
    server = MqttBroker(free_port(), '127.0.0.1')
    server.start()
    try:
        yield server
    finally:
        server.stop()


@pytest.fixture
def watch(broker: MqttBroker) -> Iterator[object]:
    from ..mqttwatch import MqttWatch
    client = MqttWatch(broker.host, broker.port, TOPIC)
    try:
        yield client
    finally:
        client.close()


def test_брокер_поднимается_и_гаснет() -> None:
    server = MqttBroker(free_port(), '127.0.0.1')
    server.start()
    assert server.listening()
    server.stop()
    assert not server.listening(), 'после stop порт обязан освободиться'


def test_выпуск_amqtt_без_исправлений_не_поднимается(monkeypatch: pytest.MonkeyPatch) -> None:
    """Выпуск с PyPI молча бросал бы хвост публикаций устройства (README, «Брокер»)."""
    import importlib.metadata

    monkeypatch.setattr(importlib.metadata, 'version', lambda _name: '0.12.1')

    with pytest.raises(RuntimeError, match='requirements.txt'):
        MqttBroker._broker_class(lambda *_: None)


def test_занятый_порт_не_принимаем_за_свой_брокер(broker: MqttBroker) -> None:
    """
    Чужой брокер на том же порту тесты приняли бы за свой и читали бы его
    топики - с retain и автодискавери из прошлой жизни.
    """
    second = MqttBroker(broker.port, '127.0.0.1')
    with pytest.raises(RuntimeError, match='уже занят'):
        second.start()


def test_команда_уходит_в_топик_автодискавери(watch) -> None:
    """
    Топик команды собирается так же, как `cmd_t` в прошивке
    (`ha/discovery_entity.cpp`): `<топик>/<сущность>/set`.
    """
    assert watch.command_topic('vac') == f'{TOPIC}/vac/set'

    watch.publish_set('vac', 1, retain=False)
    message = watch.wait_prefix(TOPIC, timeout=5)

    assert message is not None, 'подписчик не увидел собственную публикацию'
    assert message.topic == f'{TOPIC}/vac/set'
    assert message.payload == '1'


def test_чужое_дерево_не_считается_топиком_устройства(watch) -> None:
    """`waterius/stand2` начинается с тех же букв, но это другое устройство."""
    watch._client.publish(f'{TOPIC}2/ch0', '1').wait_for_publish(5)
    watch._client.publish('homeassistant/switch/x/config', '{}').wait_for_publish(5)

    assert watch.wait_prefix(TOPIC, timeout=2) is None
    assert watch.topics('homeassistant/'), 'соседнее дерево при этом видно'


def test_удалённая_сущность_не_считается_топиком(watch) -> None:
    """
    Пустая нагрузка - это удаление, а не значение: так спецификация HA MQTT
    Discovery убирает сущность, и так же брокер снимает удерживаемое сообщение.
    Считать такой топик живым значит винить прошивку за честную уборку.
    """
    config = 'homeassistant/number/waterius-1/f0/config'

    def дождаться(есть: bool) -> list[str]:
        end = time.time() + 5
        while time.time() < end:
            topics = watch.topics('homeassistant/')
            if (config in topics) == есть:
                return topics
            time.sleep(0.1)
        return watch.topics('homeassistant/')

    watch._client.publish(config, '{\"name\": \"f0\"}', retain=True).wait_for_publish(5)
    assert config in дождаться(True)

    watch._client.publish(config, '', retain=True).wait_for_publish(5)
    assert config not in дождаться(False), 'удалённая сущность осталась топиком'


def test_наблюдатель_переживает_обрыв_брокера(broker: MqttBroker, watch) -> None:
    """
    Подписка живёт внутри соединения, и клиент теряет её при обрыве.

    paho переподключается сам и молча, поэтому без подписки в on_connect стенд
    глох до конца прогона: устройство публиковало и писало в свой лог
    «Published succesfully», а тест докладывал «в брокере пусто» и винил прошивку.
    """
    watch._client.publish(f'{TOPIC}/ch0', 'до', retain=False).wait_for_publish(5)
    assert watch.wait_prefix(TOPIC, timeout=5) is not None

    broker.stop()
    broker.start()                     # тот же порт: клиент вернётся сам

    end = time.time() + 20
    while time.time() < end and watch._connects < 2:
        time.sleep(0.2)
    assert watch._connects >= 2, 'наблюдатель не переподключился'

    watch.drain()                      # старое сообщение из очереди не спутать с новым
    watch._client.publish(f'{TOPIC}/ch0', 'после', retain=False).wait_for_publish(5)
    message = watch.wait_prefix(TOPIC, timeout=10)
    assert message is not None and message.payload == 'после', (
        'после обрыва наблюдатель не подписался заново и оглох')


def test_флаг_retain_виден_только_новому_подписчику(watch) -> None:
    watch.publish_set('vac', 1, retain=True)
    live = watch.wait_prefix(TOPIC, timeout=5)
    assert live is not None
    assert not live.retain, 'живая доставка идёт с нулевым флагом'

    retained = watch.fetch_retained(TOPIC, timeout=2)
    assert [m.topic for m in retained] == [f'{TOPIC}/vac/set']
    assert all(m.retain for m in retained)


def test_подписка_стенда_не_доносится_до_устройства(broker: MqttBroker, watch) -> None:
    """
    Удерживаемое сообщение уходит только тому, кто подписан на его топик.

    Выпуск amqtt с PyPI при подключении рассылает новому клиенту
    удерживаемые сообщения по чужим фильтрам - и Ватериус, слушающий только
    своё дерево, получил бы автодискавери из `homeassistant/`, а следом,
    по своей логике, стёр бы у него флаг retain.
    """
    import paho.mqtt.client as mqtt

    watch._client.publish('homeassistant/switch/w/vac/config', '{}',
                          retain=True).wait_for_publish(5)
    watch.publish_set('vac', 1, retain=True)          # подписчик '#' уже есть
    time.sleep(0.5)

    got: list[str] = []
    device = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='waterius')
    device.on_message = lambda _c, _u, msg: got.append(msg.topic)
    device.connect(broker.host, broker.port, 30)
    device.subscribe(f'{TOPIC}/#', qos=1)
    device.loop_start()
    time.sleep(1.5)
    device.loop_stop()
    device.disconnect()

    assert got == [f'{TOPIC}/vac/set'], (
        'устройству досталось чужое дерево: оно вычистило бы там retain')


def test_удерживаемое_отдаётся_с_тем_же_qos(broker: MqttBroker, watch) -> None:
    """
    Доставка идёт с наименьшим из двух QoS - подписки и публикации
    (MQTT 3.1.1, 3.8.4). Выпуск amqtt с PyPI отдаёт сообщение,
    опубликованное с нулём, единицей и ждёт подтверждения: Ватериус свои же
    показания в буфер 256 байт не принимает, PUBACK не шлёт, и брокер рвёт ему
    сеанс - публикации этого пробуждения пропадают целиком.
    """
    import paho.mqtt.client as mqtt

    watch._client.publish(f'{TOPIC}/ch0', '1', qos=0, retain=True).wait_for_publish(5)
    time.sleep(0.5)

    got: list[int] = []
    late = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='late')
    late.on_message = lambda _c, _u, msg: got.append(msg.qos)
    late.connect(broker.host, broker.port, 30)
    late.subscribe(f'{TOPIC}/#', qos=1)
    late.loop_start()
    time.sleep(1.5)
    late.loop_stop()
    late.disconnect()

    assert got == [0], f'удерживаемое пришло с QoS {got}, а публиковали нулём'


def test_ожидание_топика_переживает_опоздание(watch: Any) -> None:
    """
    Снимок сразу после сеанса - гонка: последние публикации устройства доходят
    до подписчика позже строки ухода в сон.
    """
    assert watch.last(f'{TOPIC}/rssi') is None

    опоздавший = threading.Timer(
        1.0, lambda: watch.publish_retained(f'{TOPIC}/rssi', '-74'))
    опоздавший.start()
    try:
        message = watch.wait_topic(f'{TOPIC}/rssi', timeout=10)
    finally:
        опоздавший.cancel()

    assert message is not None, 'опоздавшее сообщение должно дождаться'
    assert message.payload == '-74'


def test_ожидание_топика_не_висит_дольше_потолка(watch: Any) -> None:
    """Молчание не должно висеть до конца прогона."""
    начали = time.monotonic()

    assert watch.wait_topic(f'{TOPIC}/нет-такого', timeout=1.0) is None

    assert time.monotonic() - начали < 3.0


def подписчик(broker: MqttBroker, client_id: str, got: list[Any],
              qos: int = 1) -> Any:
    """Клиент вроде Ватериуса: подписка с QoS 1 и сбор пришедшего."""
    import paho.mqtt.client as mqtt

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    client.on_message = lambda _c, _u, msg: got.append(msg)
    client.connect(broker.host, broker.port, 30)
    client.subscribe(f'{TOPIC}/#', qos=qos)
    client.loop_start()
    time.sleep(0.5)
    return client


def test_живая_доставка_не_поднимает_qos_до_подписки(broker: MqttBroker, watch) -> None:
    """
    Доставка идёт с наименьшим из двух QoS (MQTT 3.1.1, 3.8.4), а выпуск amqtt
    с PyPI теряет QoS публикации по дороге и берёт QoS подписки. Цена видна на
    железе: 29 сентября уборка одного теста дала 78 публикаций в сессию
    уснувшего устройства, и брокер ждал PUBACK по двадцать секунд на каждую - тысяча
    строк трейсбеков в логе и наблюдатель, потерявший брокер по keep alive.
    """
    got: list[Any] = []
    device = подписчик(broker, 'waterius', got)

    watch._client.publish(f'{TOPIC}/ch0', '1', qos=0).wait_for_publish(5)
    time.sleep(1.0)
    device.loop_stop()
    device.disconnect()

    assert [m.qos for m in got] == [0], f'доставка с QoS {[m.qos for m in got]}'


def test_уборка_удерживаемых_идёт_мимо_эфира(broker: MqttBroker, watch) -> None:
    """
    Дерево чистится в самом брокере: публиковать под сотню пустых сообщений -
    значит послать их всем подписчикам, включая сессию уснувшего устройства.
    """
    watch._client.publish(f'{TOPIC}/ch0', '1', retain=True).wait_for_publish(5)
    time.sleep(0.5)
    watch.drain()

    снято = broker.drop_retained(TOPIC)

    assert снято == [f'{TOPIC}/ch0'], снято
    assert watch.fetch_retained(TOPIC, timeout=1.0) == []
    time.sleep(0.5)
    assert not watch.history, f'уборка ушла в эфир: {watch.history}'


def test_чужое_дерево_уборка_не_трогает(broker: MqttBroker, watch) -> None:
    watch._client.publish(f'{TOPIC}/ch0', '1', retain=True).wait_for_publish(5)
    watch._client.publish('homeassistant/sensor/w/config', '{}',
                          retain=True).wait_for_publish(5)
    time.sleep(0.5)

    broker.drop_retained(TOPIC)

    assert [m.topic for m in watch.fetch_retained('homeassistant', timeout=1.0)] == [
        'homeassistant/sensor/w/config']


def test_стенд_убирает_дерево_брокером(broker: MqttBroker) -> None:
    """У наблюдателя со своим брокером уборка не публикует ничего."""
    from ..mqttwatch import MqttWatch

    watch = MqttWatch(broker.host, broker.port, TOPIC, broker=broker)
    try:
        watch._client.publish(f'{TOPIC}/ch0', '1', retain=True).wait_for_publish(5)
        time.sleep(0.5)
        watch.drain()

        снято = watch.clear_retained_tree(TOPIC)

        assert снято == [f'{TOPIC}/ch0'], снято
        time.sleep(0.5)
        assert not watch.history, f'уборка ушла в эфир: {watch.history}'
    finally:
        watch.close()


# --- устройство на голом сокете ---

def _пакет(kind: int, body: bytes) -> bytes:
    size, head = len(body), b''
    while True:
        byte, size = size & 0x7F, size >> 7
        head += bytes([byte | (0x80 if size else 0)])
        if not size:
            return bytes([kind]) + head + body


def _строка(text: str) -> bytes:
    raw = text.encode()
    return len(raw).to_bytes(2, 'big') + raw


ПУБЛИКАЦИЙ = 83
ПРОЩАНИЕ = (_пакет(0xA2, (2).to_bytes(2, 'big') + _строка(f'{TOPIC}/#'))
            + _пакет(0xE0, b''))


def устройство(broker: MqttBroker, client_id: str = 'waterius-1') -> Any:
    """Сокет, прошедший CONNECT, - как PubSubClient прошивки."""
    import socket

    sock = socket.create_connection((broker.host, broker.port), timeout=5)
    sock.sendall(_пакет(0x10, _строка('MQTT') + bytes([4, 0x02])
                        + (15).to_bytes(2, 'big') + _строка(client_id)))
    assert sock.recv(4)[:1] == b'\x20', 'нет CONNACK'
    return sock


def публикации(n: int = ПУБЛИКАЦИЙ) -> bytes:
    return b''.join(_пакет(0x31, _строка(f'{TOPIC}/f{i:02d}') + str(i).encode())
                    for i in range(n))


def дождаться_всех(watch: Any, n: int = ПУБЛИКАЦИЙ, timeout: float = 5.0) -> list[str]:
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        topics = watch.topics(f'{TOPIC}/')
        if len(topics) >= n:
            return topics
        time.sleep(0.1)
    return watch.topics(f'{TOPIC}/')


@pytest.mark.parametrize('хвост', [ПРОЩАНИЕ, b''], ids=['DISCONNECT', 'только_FIN'])
def test_пачка_перед_концом_соединения_доходит_целиком(
        broker: MqttBroker, watch: Any, хвост: bytes) -> None:
    """
    Публикации, пришедшие в сокет одной пачкой с концом соединения, не теряются.

    Так их отдаёт Ватериус на слабом эфире: сегмент застрял, TCP перепослал, и
    хвост публикаций приехал вместе с DISCONNECT. Выпуск amqtt с PyPI разносит
    из такой пачки одну публикацию из 83 (docs: 07_mqtt-tail-loss.md).
    """
    sock = устройство(broker)
    sock.sendall(публикации() + хвост)
    sock.close()

    topics = дождаться_всех(watch)

    assert len(topics) == ПУБЛИКАЦИЙ, (
        f'брокер разослал {len(topics)} из {ПУБЛИКАЦИЙ}, последний - '
        f'{topics[-1] if topics else None}')


def test_брокер_докладывает_об_оборванном_потоке(broker: MqttBroker, watch: Any) -> None:
    """
    Устройство уснуло посреди потока: брокер не получил ни DISCONNECT, ни FIN.

    По логу устройства такое не отличить от потери в самом брокере - там
    напечатаны все публикации. Отличает запись брокера: соединение открыто, и
    видно, на каком топике поток встал.
    """
    sock = устройство(broker)
    try:
        sock.sendall(публикации(10))
        assert len(дождаться_всех(watch, 10)) == 10

        note = broker.note()
    finally:
        sock.close()

    assert 'waterius-1' in note and '10 публикаций' in note, note
    assert f'{TOPIC}/f09' in note, note
    assert 'открыто' in note, note


def test_брокер_докладывает_о_штатном_прощании(broker: MqttBroker, watch: Any) -> None:
    sock = устройство(broker)
    sock.sendall(публикации(5) + ПРОЩАНИЕ)
    sock.close()
    assert len(дождаться_всех(watch, 5)) == 5

    end = time.monotonic() + 5
    while 'DISCONNECT' not in broker.note() and time.monotonic() < end:
        time.sleep(0.1)

    note = broker.note()
    assert '5 публикаций' in note and 'DISCONNECT' in note, note
    assert 'hil-' not in note, f'свои клиенты стенда в отчёт не нужны: {note}'
