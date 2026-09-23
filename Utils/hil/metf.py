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

Третье правило - про то, что повторять нельзя. Повтор осмыслен, только если
запрос заведомо не доехал: плата отказала в соединении или не ответила на SYN.
`ReadTimeout` означает обратное - запрос ушёл, ответа нет, - и для `/pulse` это
принципиально: импульс мог быть выдан, и повтор нажмёт кнопку второй раз.
"""

from __future__ import annotations

import time
from collections.abc import Callable
from typing import Any

import requests
from loguru import logger
from metf_python_client import METFClient

ATTEMPTS = 3                 # первая попытка и два повтора
PAUSE_S = 1.0                # пауза перед повтором, дальше удваивается
DEAD_AFTER_S = 60.0          # столько молчания - и прогон останавливается

# Что считаем осечкой связи, а не ошибкой вызова: таймауты, обрыв соединения,
# «host is down» от стека. Всё остальное - ошибка теста, её не глушим
NETWORK_ERRORS = (requests.RequestException, OSError)

# Что можно повторять: соединение не установилось, значит плата запроса не
# видела. `ConnectTimeout` - наследник `ConnectionError`, поэтому попадает сюда,
# а `ReadTimeout` - нет, и это ровно то поведение, которое нужно
SAFE_TO_REPEAT = (requests.ConnectionError,)

# Ответ на `/pulse` - расписка о приёме, он не ждёт конца выдержки: медиана
# замера 16 мс. Пять секунд с запасом хватит и на шаткую связь
PULSE_ANSWER_TIMEOUT_S = 5.0


class MetfGone(Exception):
    """METF не отвечает дольше `DEAD_AFTER_S`: прогон продолжать бессмысленно."""


class MetfTooOld(Exception):
    """На плате прошивка старше той, на которую рассчитан стенд."""


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
            return self._retry(name, target, args, kwargs)

        call.__name__ = name
        return call

    def pulse(self, pin: int, value: int, duration_ms: int) -> None:
        """
        Выдержку отмеряет плата, конец импульса ждёт стенд: `POST /pulse`
        (протокол 8).

        Клиент 0.4 этого метода не знает, поэтому зовём эндпоинт напрямую - но
        через общий `_retry`, иначе самое частое действие стенда (нажатие
        кнопки, импульсы счётчиков) остаётся единственным без повторов. Так и
        было: девять ошибок прогона пришли отсюда.

        Плата отвечает сразу - `202` и заказанная выдержка в теле, - а паузу до
        конца импульса держим здесь. Раньше её держала плата: ответ был отложен
        до конца выдержки. Так было соблазнительно - ответ означал «линия
        отпущена», - но отложенный ответ уходит не когда готов, а на ближайшем
        опросе AsyncTCP, то есть примерно дважды в секунду. Замер на плате
        (импульс 20 мс, 20 повторов) дал опоздание 240-336 мс, и эта добавка
        растягивала паузы между импульсами: `D7` и `D9` начали видеть двойной
        счёт, потому что attiny перестал сливать два замыкания в один импульс.

        Ждём после ответа, а не от момента запроса: линия отпускается через
        `duration_ms` после того, как плата приняла запрос, то есть позже
        нашего вызова на полдороги сети. Лишние миллисекунды ожидания безвредны,
        а вот проснуться раньше времени - значит трогать ещё занятую линию.

        Повторяется только отказ соединения: см. `SAFE_TO_REPEAT`.
        """
        root = self._api._root
        session = self._api._sess
        answer = self._retry(
            'pulse', session.post,
            (f'{root}/pulse',),
            {'data': {'pin': pin, 'value': value, 'duration_ms': duration_ms},
             'timeout': PULSE_ANSWER_TIMEOUT_S},
            repeatable=SAFE_TO_REPEAT)
        answer.raise_for_status()
        if answer.status_code != 202:
            raise MetfTooOld(
                f'стенд: METF {self._host} ответила на /pulse кодом '
                f'{answer.status_code}, а не 202: на плате прошивка старше '
                f'протокола 8. Выдержку она отмеряет ответом, а не телом, и '
                f'паузы между импульсами поедут. Обновите прошивку платы.')
        time.sleep(duration_ms / 1000.0)

    def _retry(self, name: str, target: Callable[..., Any],
               args: tuple[Any, ...], kwargs: dict[str, Any],
               repeatable: tuple[type[BaseException], ...] = NETWORK_ERRORS) -> Any:
        pause = self._pause
        last: Exception | None = None
        for attempt in range(1, self._attempts + 1):
            try:
                answer = target(*args, **kwargs)
            except NETWORK_ERRORS as err:
                last = err
                again = isinstance(err, repeatable)
                self._note_failure(name, err, attempt,
                                   self._attempts if again else 1)
                if not again:
                    break
                if attempt < self._attempts:
                    time.sleep(pause)
                    pause *= 2
                continue
            self._note_success()
            return answer

        assert last is not None
        raise last

    def _note_failure(self, name: str, err: Exception,
                      attempt: int, attempts: int) -> None:
        now = time.time()
        if self._down_since is None:
            self._down_since = now
        silent = now - self._down_since
        if silent > self._dead_after:
            raise MetfGone(
                f'стенд: METF {self._host} молчит {silent:.0f} с - прогон остановлен. '
                f'Последняя ошибка на {name}(): {err}')
        if attempts == 1:
            logger.warning(f'METF: {name}() - {type(err).__name__}, '
                           f'повторять нельзя: {err}')
        else:
            logger.warning(f'METF: {name}() - {type(err).__name__}, '
                           f'попытка {attempt} из {attempts}: {err}')

    def _note_success(self) -> None:
        if self._down_since is None:
            return
        logger.warning(f'METF: связь вернулась через {time.time() - self._down_since:.1f} с')
        self._down_since = None
