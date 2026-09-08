"""
Подписка на топики Ватериуса и удалённые команды.

Два разных дела в одном месте: слушать, что публикует устройство (данные и
автодискавери), и присылать ему команды через `<топик>/<параметр>/set`. Команда
применяется в том же сеансе, где получена: подписка выполняется до отправки
данных, поэтому удерживаемое сообщение подхватывается сразу, а после применения
данные уходят повторно.

Флаг retain в живой доставке всегда нулевой - брокер выставляет его только
тому, кто подписался уже после публикации (MQTT 3.1.1, 3.3.1.3). Поэтому
настройка mqtt_retain проверяется отдельным подписчиком: `fetch_retained`.
"""

from __future__ import annotations

import json
import queue
import threading
import time
from dataclasses import dataclass
from typing import Any

import paho.mqtt.client as mqtt
from loguru import logger


@dataclass
class Message:
    topic: str
    payload: str
    retain: bool

    def json(self) -> Any:
        return json.loads(self.payload)


class MqttWatch:
    """Подписчик на всё дерево брокера плюс публикация команд."""

    def __init__(self, host: str, port: int = 1883, topic: str = 'waterius') -> None:
        self.host = host
        self.port = port
        self.topic = topic.rstrip('/')
        self.messages: queue.Queue[Message] = queue.Queue()
        self.history: list[Message] = []
        self._lock = threading.Lock()

        # В paho 2.x конструктор требует версию API обратных вызовов явно.
        self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                                   client_id=f'hil-{int(time.time())}')
        self._client.on_message = self._on_message
        self._client.connect(host, port, keepalive=30)
        self._client.subscribe('#')
        self._client.loop_start()

    def _on_message(self, _client: Any, _userdata: Any, msg: mqtt.MQTTMessage) -> None:
        message = Message(msg.topic, msg.payload.decode(errors='replace'), bool(msg.retain))
        with self._lock:
            self.history.append(message)
        self.messages.put(message)

    # --- ожидание ---

    def wait_prefix(self, prefix: str, timeout: float = 60.0) -> Message | None:
        """
        Дождаться сообщения из дерева устройства.

        Именно по началу топика, а не по концу: суффикс совпадает и у чужого
        сообщения, и у автодискавери, который уходит в `homeassistant/`, -
        проверка «хоть что-то приехало» так зеленела бы всегда.
        """
        deadline = time.time() + timeout
        while True:
            with self._lock:
                root = prefix.rstrip('/')
                for message in reversed(self.history):
                    if message.topic == root or message.topic.startswith(root + '/'):
                        return message
            if time.time() >= deadline:
                return None
            time.sleep(0.5)

    def fetch_retained(self, prefix: str, timeout: float = 5.0) -> list[Message]:
        """
        Что лежит в брокере удерживаемым - глазами нового подписчика.

        Иначе флаг retain не проверить: тому, кто уже подписан, брокер
        доставляет сообщение с нулевым флагом, и утверждение `message.retain`
        падало бы независимо от настройки в прошивке.
        """
        root = prefix.rstrip('/')
        found: list[Message] = []
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                             client_id=f'hil-retained-{int(time.time())}')
        client.on_message = lambda _c, _u, msg: found.append(
            Message(msg.topic, msg.payload.decode(errors='replace'), bool(msg.retain)))
        client.connect(self.host, self.port, keepalive=30)
        # Одного фильтра достаточно: `#` покрывает и сам корень, куда
        # прошивка кладёт показания одним объектом (MQTT 3.1.1, 4.7.1.2).
        client.subscribe(f'{root}/#', qos=1)
        client.loop_start()
        time.sleep(timeout)
        client.loop_stop()
        client.disconnect()
        return found

    def last(self, topic: str) -> Message | None:
        """Последнее сообщение в точности этого топика."""
        with self._lock:
            for message in reversed(self.history):
                if message.topic == topic:
                    return message
        return None

    def topics(self, prefix: str = '') -> list[str]:
        with self._lock:
            return sorted({m.topic for m in self.history if m.topic.startswith(prefix)})

    def drain(self) -> None:
        with self._lock:
            self.history.clear()
        while not self.messages.empty():
            self.messages.get_nowait()

    # --- команды устройству ---

    def command_topic(self, name: str) -> str:
        """
        Топик команды - тот же, что прошивка объявляет в автодискавери:
        `cmd_t` собирается как `<топик>/<сущность>/set`
        (`ha/discovery_entity.cpp`). Лишний сегмент в середине прошивка бы
        проглотила - она подписана на `<топик>/#`, а имя параметра берёт из
        предпоследнего сегмента, - и тест прошёл бы мимо настоящего пути HA.
        """
        return f'{self.topic}/{name}/set'

    def publish_set(self, name: str, value: Any, retain: bool = True) -> None:
        """
        Прислать настройку так, как это делает Home Assistant.

        retain=True по умолчанию: ЕСП живёт секунды и подписывается только на
        время сеанса, обычное сообщение она просто не застанет.
        """
        topic = self.command_topic(name)
        logger.info(f'MQTT -> {topic} = {value}')
        self._client.publish(topic, str(value), retain=retain)

    def clear_retained(self, topic: str) -> None:
        self._client.publish(topic, '', retain=True)

    def close(self) -> None:
        self._client.loop_stop()
        self._client.disconnect()
