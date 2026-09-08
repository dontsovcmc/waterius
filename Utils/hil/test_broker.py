"""
Брокер и подписчик - без железа.

Проверяется то, на чём блок MQTT стоит целиком: брокер поднимается в процессе
тестов, команда уходит в тот же топик, который прошивка объявляет Home
Assistant, а удерживаемое сообщение видно только новому подписчику. Последнее
неочевидно и стоило теста I7 ложного падения: живая доставка идёт с нулевым
флагом retain у любого брокера (MQTT 3.1.1, 3.3.1.3).
"""

from __future__ import annotations

import socket
import time
from typing import Iterator

import pytest

from .broker import MqttBroker

if not MqttBroker.available():
    pytest.skip('нет amqtt: pip install -r Utils/hil/requirements.txt',
                allow_module_level=True)

TOPIC = 'waterius/stand'


def free_port() -> int:
    """Порт, который сейчас никто не слушает: 1883 может быть занят прогоном."""
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return int(sock.getsockname()[1])


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
    from .mqttwatch import MqttWatch
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

    Без заплаты (broker.py) amqtt при подключении рассылает новому клиенту
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
    (MQTT 3.1.1, 3.8.4). Без заплаты (broker.py) amqtt отдавал бы сообщение,
    опубликованное с нулём, единицей и ждал бы подтверждения: Ватериус свои же
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
