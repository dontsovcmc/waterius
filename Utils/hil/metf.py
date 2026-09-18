"""
METF с повторами: управляющий канал стенда идёт по радио.

METF - клиент Wi-Fi домашней сети, и тот же эфир тесты ломают намеренно: гасят
и поднимают точку доступа стенда, меняют ей канал, а Ватериус в режиме
настройки поднимает свою. Плата при этом отваливается, а `metf_python_client`
ходит с таймаутом в 3 секунды и без единого повтора, поэтому одна осечка роняла
тест, а отлучка подлиннее - весь хвост прогона: 23 одинаковых traceback подряд,
час впустую.

Отсюда два правила. Короткая осечка - это повтор, а не падение. Долгое молчание
платы - это конец прогона сразу, с внятным текстом: без METF стенд всё равно
ничего не может, и час одинаковых ошибок никому не нужен.
"""

from __future__ import annotations

import time
from typing import Any, Callable

import requests
from loguru import logger
from metf_python_client import METFClient

ATTEMPTS = 3                 # первая попытка и два повтора
PAUSE_S = 1.0                # пауза перед повтором, дальше удваивается
DEAD_AFTER_S = 60.0          # столько молчания - и прогон останавливается

# Что считаем осечкой связи, а не ошибкой вызова: таймауты, обрыв соединения,
# «host is down» от стека. Всё остальное - ошибка теста, её не глушим
NETWORK_ERRORS = (requests.RequestException, OSError)


class MetfGone(Exception):
    """METF не отвечает дольше `DEAD_AFTER_S`: прогон продолжать бессмысленно."""


class Metf:
    """
    `METFClient`, у которого каждый вызов переживает короткий обрыв связи.

    Обёртка прозрачная: имена и аргументы - клиента, поэтому в коде стенда
    ничего не меняется. Каждая осечка пишется в лог предупреждением, даже если
    повтор удался, - иначе шаткая связь остаётся невидимой до самого обвала.
    """

    def __init__(self, host: str, attempts: int = ATTEMPTS,
                 pause: float = PAUSE_S, dead_after: float = DEAD_AFTER_S) -> None:
        self._api = METFClient(host)
        self._host = host
        self._attempts = attempts
        self._pause = pause
        self._dead_after = dead_after
        self._down_since: float | None = None

    def __getattr__(self, name: str) -> Any:
        target = getattr(self._api, name)
        if not callable(target):
            return target

        def call(*args: Any, **kwargs: Any) -> Any:
            return self._retry(name, target, *args, **kwargs)

        call.__name__ = name
        return call

    def _retry(self, name: str, target: Callable[..., Any],
               *args: Any, **kwargs: Any) -> Any:
        pause = self._pause
        last: Exception | None = None
        for attempt in range(1, self._attempts + 1):
            try:
                answer = target(*args, **kwargs)
            except NETWORK_ERRORS as err:
                last = err
                self._note_failure(name, err, attempt)
                if attempt < self._attempts:
                    time.sleep(pause)
                    pause *= 2
                continue
            self._note_success()
            return answer

        assert last is not None
        raise last

    def _note_failure(self, name: str, err: Exception, attempt: int) -> None:
        now = time.time()
        if self._down_since is None:
            self._down_since = now
        silent = now - self._down_since
        if silent > self._dead_after:
            raise MetfGone(
                f'стенд: METF {self._host} молчит {silent:.0f} с - прогон остановлен. '
                f'Последняя ошибка на {name}(): {err}')
        logger.warning(f'METF: {name}() - {type(err).__name__}, '
                       f'попытка {attempt} из {self._attempts}: {err}')

    def _note_success(self) -> None:
        if self._down_since is None:
            return
        logger.warning(f'METF: связь вернулась через {time.time() - self._down_since:.1f} с')
        self._down_since = None
