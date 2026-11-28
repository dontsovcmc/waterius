"""
Сервер времени стенда: он на роутере, команда консоли `ntp` (прошивка `2.5.0-waterius.1`).

Время устройству отдаёт роутер стенда, а не эта машина и не интернет. Так тесты
синхронизации остаются локальными: ни один из них не зависит от того, доступен
ли пул `ru.pool.ntp.org` и что там сейчас за время.

Своих часов у сервера нет - момент назначает тест. Это не ограничение, а рычаг:
можно назначить заведомо узнаваемое время и убедиться, что устройство взяло
именно его, а не своё оценочное.
"""

from __future__ import annotations

import json
from typing import Any

from loguru import logger

from .router import NatRouter, RouterError


class RouterClock:
    """`ntp start|time|stop|drop|stat` консоли роутера."""

    def __init__(self, router: NatRouter) -> None:
        self._router = router

    # --- управление ---

    def start(self, epoch: int) -> None:
        """Назначить время и начать отвечать."""
        self._ok(f'ntp start {int(epoch)}')
        logger.info(f'сервер времени поднят, epoch={int(epoch)}')

    def set_time(self, epoch: int) -> None:
        """Переставить часы, не трогая слушателя."""
        self._ok(f'ntp time {int(epoch)}')

    def stop(self) -> None:
        """Освободить порт: устройство получит отказ сразу."""
        self._ok('ntp stop')

    def drop(self, on: bool = True) -> None:
        """
        Слушать, но молчать.

        Так выглядит недоступный сервер в интернете: устройство ждёт таймаут,
        а не получает отказ порта. Для проверки поведения при недоступном NTP
        это ближе к жизни, чем закрытый порт.
        """
        self._ok(f'ntp drop {1 if on else 0}')

    # --- наблюдение ---

    def stat(self) -> dict[str, Any]:
        answer = self._router.cmd('ntp stat')
        try:
            return dict(json.loads(answer))
        except ValueError as err:
            raise RouterError(f'роутер ответил на «ntp stat» не JSON: {answer!r}') from err

    @property
    def requests_seen(self) -> int:
        """Сколько запросов пришло на сервер - по нему видно, ходило ли устройство."""
        return int(self.stat()['requests'])

    def available(self) -> bool:
        """Есть ли на роутере сервер времени: его нет в прошивках без `-waterius`."""
        try:
            self.stat()
        except RouterError:
            return False
        return True

    def _ok(self, line: str) -> None:
        answer = self._router.cmd(line)
        if answer != 'ok':
            raise RouterError(f'роутер не выполнил «{line}»: {answer}')
