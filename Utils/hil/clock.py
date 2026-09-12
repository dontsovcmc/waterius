"""
Сервер времени стенда: он же плата METF, протокол 6.

Время устройству отдаёт плата, а не эта машина и не интернет. Так тесты
синхронизации остаются локальными: ни один из них не зависит от того, доступен
ли пул `ru.pool.ntp.org` и что там сейчас за время.

Своих часов у платы нет - момент назначает тест. Это не ограничение, а рычаг:
можно назначить заведомо узнаваемое время и убедиться, что устройство взяло
именно его, а не своё оценочное.
"""

from __future__ import annotations

from typing import Any

import requests
from loguru import logger

TIMEOUT = 5.0
NTP_PROTOCOL = 6        # версия протокола METF, в которой появился /ntp


class BoardClock:
    """`POST /ntp` и `GET /ntp/stat` платы METF."""

    def __init__(self, host: str) -> None:
        self.host = host
        self._root = f'http://{host}'

    # --- управление ---

    def start(self, epoch: int) -> None:
        """Назначить время и начать отвечать."""
        self._post(action='start', epoch=int(epoch))
        logger.info(f'сервер времени поднят, epoch={int(epoch)}')

    def set_time(self, epoch: int) -> None:
        """Переставить часы, не трогая слушателя."""
        self._post(action='time', epoch=int(epoch))

    def stop(self) -> None:
        """Освободить порт: устройство получит отказ сразу."""
        self._post(action='stop')

    def drop(self, on: bool = True) -> None:
        """
        Слушать, но молчать.

        Так выглядит недоступный сервер в интернете: устройство ждёт таймаут,
        а не получает отказ порта. Для проверки поведения при недоступном NTP
        это ближе к жизни, чем закрытый порт.
        """
        self._post(action='drop', value=1 if on else 0)

    # --- наблюдение ---

    def stat(self) -> dict[str, Any]:
        answer = requests.get(f'{self._root}/ntp/stat', timeout=TIMEOUT)
        answer.raise_for_status()
        return answer.json()

    @property
    def requests_seen(self) -> int:
        """Сколько запросов пришло на плату - по нему видно, ходило ли устройство."""
        return int(self.stat()['requests'])

    def available(self) -> bool:
        """Есть ли на плате сервер времени: он появился в шестой версии протокола."""
        answer = requests.get(f'{self._root}/version', timeout=TIMEOUT)
        answer.raise_for_status()
        return int(answer.text.strip()) >= NTP_PROTOCOL

    def _post(self, **params: Any) -> None:
        answer = requests.post(f'{self._root}/ntp', data=params, timeout=TIMEOUT)
        answer.raise_for_status()
