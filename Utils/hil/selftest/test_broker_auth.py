"""
Брокер с паролями - без устройства.

I16 опирается на поведение amqtt, а не на документацию: клиента без имени
брокер пускает, клиента с именем - только с верным паролем. Если amqtt это
поменяет, стенд узнает здесь, а не по красному I16 на живом железе.
"""

from __future__ import annotations

import time

import pytest

mqtt = pytest.importorskip('paho.mqtt.client')
pytest.importorskip('amqtt')

from ..broker import MqttBroker  # noqa: E402
from . import free_port  # noqa: E402

USER = 'waterius'
PASSWORD = 'Aa0' * 21 + 'Z'          # 64 символа: столько генерирует HA (#301)


@pytest.fixture(scope='module')
def secured() -> MqttBroker:
    broker = MqttBroker(free_port(), '127.0.0.1', users={USER: PASSWORD})
    broker.start()
    yield broker
    broker.stop()


def accepted(broker: MqttBroker, username: str | None = None,
             password: str | None = None) -> bool:
    """Пустил ли брокер клиента: по коду CONNACK, а не по отсутствию исключения."""
    result: dict[str, object] = {}
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id=f'selftest-{time.time_ns()}')
    if username is not None:
        client.username_pw_set(username, password)
    client.on_connect = lambda _c, _u, _f, reason, _p: result.setdefault('reason', reason)
    client.connect(broker.host, broker.port, keepalive=5)
    client.loop_start()
    deadline = time.time() + 5
    while 'reason' not in result and time.time() < deadline:
        time.sleep(0.05)
    client.loop_stop()
    client.disconnect()
    assert 'reason' in result, 'брокер не ответил на CONNECT'
    return not result['reason'].is_failure


def test_anonymous_client_is_accepted(secured: MqttBroker) -> None:
    assert accepted(secured)


def test_long_password_is_accepted(secured: MqttBroker) -> None:
    assert len(PASSWORD) == 64
    assert accepted(secured, USER, PASSWORD)


def test_wrong_password_is_refused(secured: MqttBroker) -> None:
    assert not accepted(secured, USER, PASSWORD[:-1] + 'x')
