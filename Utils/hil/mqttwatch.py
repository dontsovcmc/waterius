"""
Подписка на топики Ватериуса и удалённые команды.

Два разных дела в одном месте: слушать, что публикует устройство (данные и
автодискавери), и присылать ему команды через `<топик>/<параметр>/set`. Команда
применяется в том же сеансе, где получена: подписка выполняется до отправки
данных, поэтому удерживаемое сообщение подхватывается сразу, а после применения
данные уходят повторно.

Флаг retain у пришедшего сообщения сохраняем: по нему проверяется настройка
mqtt_retain, и это честнее, чем перезапускать брокер.
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
        self.topic = topic.rstrip('/')
        self.messages: queue.Queue[Message] = queue.Queue()
        self.history: list[Message] = []
        self._lock = threading.Lock()

        self._client = mqtt.Client(client_id=f'hil-{int(time.time())}')
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

    def wait_topic(self, suffix: str, timeout: float = 60.0) -> Message | None:
        """Дождаться сообщения, топик которого заканчивается на suffix."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                for message in reversed(self.history):
                    if message.topic.endswith(suffix):
                        return message
            time.sleep(0.5)
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

    def publish_set(self, name: str, value: Any, device: str,
                    retain: bool = True) -> None:
        """
        Прислать настройку так, как это делает Home Assistant.

        retain=True по умолчанию: ЕСП живёт секунды и подписывается только на
        время сеанса, обычное сообщение она просто не застанет.
        """
        topic = f'{self.topic}/{device}/{name}/set'
        logger.info(f'MQTT -> {topic} = {value}')
        self._client.publish(topic, str(value), retain=retain)

    def clear_retained(self, topic: str) -> None:
        self._client.publish(topic, '', retain=True)

    def close(self) -> None:
        self._client.loop_stop()
        self._client.disconnect()
