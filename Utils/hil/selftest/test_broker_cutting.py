"""
Рвущий брокер - без железа.

На нём стоит проверка честного статуса MQTT (I20): если брокер не рвёт
соединение или рвёт его раньше времени, тест винит прошивку не в том.
"""

from __future__ import annotations

import threading
from collections.abc import Iterator

import paho.mqtt.client as mqtt
import pytest

from ..broker import CuttingBroker
from . import free_port


@pytest.fixture
def cutting() -> Iterator[CuttingBroker]:
    server = CuttingBroker(free_port(), '127.0.0.1')
    server.start()
    try:
        yield server
    finally:
        server.stop()


def connect(broker: CuttingBroker) -> tuple[mqtt.Client, threading.Event]:
    gone = threading.Event()
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='selftest-cutting')
    client.on_disconnect = lambda *_: gone.set()
    client.connect(broker.host, broker.port, keepalive=5)
    client.loop_start()
    return client, gone


def test_подключение_и_подписка_проходят(cutting: CuttingBroker) -> None:
    client, gone = connect(cutting)
    try:
        subscribed = threading.Event()
        client.on_subscribe = lambda *_: subscribed.set()
        client.subscribe('waterius/stand/#', qos=1)
        assert subscribed.wait(5), 'SUBACK не пришёл'

        client.publish('waterius/stand', '{}').wait_for_publish(5)
        assert not gone.wait(1), 'публикация мимо дерева не должна рвать соединение'
        assert not cutting.cut.is_set()
    finally:
        client.loop_stop()
        client.disconnect()


def test_публикация_в_дерево_рвёт_соединение(cutting: CuttingBroker) -> None:
    client, gone = connect(cutting)
    try:
        client.publish('homeassistant/sensor/x/voltage/config', '{}')
        assert cutting.cut.wait(5), 'брокер не заметил публикацию'
        assert gone.wait(5), 'соединение осталось открытым'
    finally:
        client.loop_stop()
        client.disconnect()
