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

# Версии протокола платы: `GET /version`, разбор - в `docs/api.md` репозитория
# metf. Стенд опирается на три рубежа
PULSE_PROTOCOL = 8       # `/pulse` отвечает распиской, выдержку держит стенд
WIFI_PROTOCOL = 9        # `/wifi`: плату можно увести в другую сеть без пайки
PROBLEM_PROTOCOL = 10    # `problem`: причина обрыва словами, а не кодом ядра

# Ниже этого запаса связь с платой рвётся под нагрузкой эфира, а тесты её ломают
# намеренно. Замеры прогонов: на -68 дБм прогон живой, на -72...-82 METF
# отваливалась и уносила с собой весь хвост
WEAK_RSSI = -70

# Каналы 2,4 ГГц шириной 20 МГц стоят через 5 МГц, поэтому не перекрываются
# только 1, 6 и 11. Частичное перекрытие хуже общего канала: на общем платы
# слышат друг друга и делят эфир по очереди, а на соседнем - не слышат и бьют
# одновременно, отчего пакеты гибнут
CHANNEL_GAP = 5

# Чтение состояния платы: короткий запрос, повторяемый без последствий
GET_TIMEOUT_S = 5.0

# `GET /read` отдаёт накопленный лог и тут же осушает кольцо, поэтому ответ на
# него существует в одном экземпляре. Таймаут чтения означает, что запрос дошёл,
# а ответ - нет: строки уже вычерпаны платой и повтором не вернутся.
#
# Срок щедрый, и это не про терпение. Дыра одинаково смертельна любого размера,
# значит выигрывать надо в их числе: пока срок больше пары перепосылок TCP,
# потерянный пакет (замер 24 сентября: 3,3 % до METF) лечится самим протоколом и
# дыры не даёт. Потолок ставит кольцо платы - оно вмещает около пяти секунд
# сеанса с MQTT, поэтому ждать дольше уже опаснее, чем не дождаться
READ_TIMEOUT_S = 4.0


# Ответ на `/pulse` - расписка о приёме, он не ждёт конца выдержки: медиана
# замера 16 мс. Пять секунд с запасом хватит и на шаткую связь
PULSE_ANSWER_TIMEOUT_S = 5.0


class MetfGone(Exception):
    """METF не отвечает дольше `DEAD_AFTER_S`: прогон продолжать бессмысленно."""


class MetfTooOld(Exception):
    """На плате прошивка старше той, на которую рассчитан стенд."""


class MetfOnStandAp(Exception):
    """METF сидит на точке стенда - на той самой, которую тесты гасят."""


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
        # Последняя отлучка платы: когда и на сколько. По ней объясняются
        # потери лога - кольцо METF вмещает примерно один сеанс (см. README)
        self.last_stall: tuple[float, float] | None = None
        # Сколько раз ответ на `GET /read` потерялся по дороге: столько дыр в
        # логе, о которых счётчик потерь платы не знает - кольцо не переполнялось
        self.log_holes = 0

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

    def serial_read(self) -> str:
        """
        Забрать накопленный лог (`GET /read`).

        Свой метод, а не вызов клиента через обёртку, ради одного: этот запрос
        нельзя повторять после таймаута чтения. Плата осушает кольцо, отдавая
        ответ, и если ответ не доехал, строки потеряны - повтор вернёт уже
        следующие. Прежде повтор шёл молча, и тест падал позже и не там:
        «сеанс не пришёл», хотя сеанс был, а пропала его первая строка.
        """
        try:
            answer = self._retry('serial_read', self._api._sess.get,
                                 (f'{self._api._root}/read',),
                                 {'timeout': READ_TIMEOUT_S},
                                 repeatable=SAFE_TO_REPEAT)
        except requests.ConnectionError:
            raise                      # соединения не было: строки ещё на плате
        except NETWORK_ERRORS:
            self.log_holes += 1
            logger.warning(
                'METF: ответ на чтение лога не доехал - плата уже очистила '
                'кольцо, и эти строки потеряны')
            raise
        answer.raise_for_status()
        return answer.text

    def version(self) -> int:
        """Версия протокола платы (`GET /version`)."""
        return int(self._get('version').text.strip())

    def wifi(self) -> dict[str, Any]:
        """
        Состояние сети платы (`GET /wifi`, протокол 9).

        Пароль плата не отдаёт. До девятого протокола ручки нет вовсе - там
        сеть выбиралась только сборкой, и спрашивать нечего.
        """
        return self._get('wifi').json()

    def _get(self, path: str) -> Any:
        root = self._api._root
        answer = self._retry(f'GET /{path}', self._api._sess.get,
                             (f'{root}/{path}',), {'timeout': GET_TIMEOUT_S})
        answer.raise_for_status()
        return answer

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
        silent = time.time() - self._down_since
        self.last_stall = (self._down_since, silent)
        logger.warning(f'METF: связь вернулась через {silent:.1f} с')
        self._down_since = None

    def stall_since(self, mark: float) -> tuple[float, float] | None:
        """Отлучка платы после момента `mark`, если она была."""
        if self.last_stall and self.last_stall[0] >= mark:
            return self.last_stall
        return None


def check(api: Metf, host: str, stand_ssid: str = '', stand_channel: int = 0) -> str:
    """
    Досмотр платы перед прогоном: протокол, сеть, запас сигнала.

    Возвращает строку для лога прогона. Две беды считаются смертельными и
    останавливают запуск, остальное идёт предупреждением: прогон стоит начинать
    только с платой, которой можно верить, но лишний отказ у железа дороже.

    Смертельно первое: протокол младше восьмого. Там `/pulse` отвечал концом
    импульса, ответ опаздывал на четверть секунды (замер - в `Metf.pulse`), и
    паузы между импульсами едут молча - падать будут тесты счётчиков, а виновата
    будет плата.

    Смертельно второе: плата сидит на точке стенда. Управляющий канал не должен
    идти по тому, что тесты ломают: первое же `ap_off` уносит вместе с
    устройством и саму METF, а прогон встаёт с ошибками, в которых прошивка
    Ватериуса ни при чём. Так и было - плата осталась на `waterius_stand` после
    прерванного прогона и вернулась только через проброс порта на роутере.
    """
    version = api.version()
    if version < PULSE_PROTOCOL:
        raise MetfTooOld(
            f'стенд: METF {host} отвечает протоколом {version}, нужен '
            f'{PULSE_PROTOCOL} и выше: до восьмого плата держала ответ до конца '
            f'импульса, и паузы между импульсами поедут на четверть секунды. '
            f'Обновите прошивку платы (README, «Прошивка METF»).')

    if version < WIFI_PROTOCOL:
        logger.info(f'стенд: METF {host} - есть, протокол {version}')
        return f'протокол {version}'

    net = api.wifi()
    ssid = str(net.get('ssid', ''))
    if stand_ssid and ssid == stand_ssid:
        raise MetfOnStandAp(
            f'стенд: METF {host} сидит на точке стенда «{ssid}» - на той самой, '
            f'которую тесты гасят и перенастраивают. Верните плату в домашнюю '
            f'сеть: curl -d action=forget http://{host}/wifi')

    parts = [f'протокол {version}', f'сеть «{ssid}»']
    rssi = net.get('rssi')
    if rssi is not None:
        parts.append(f'{rssi} дБм')
    if version >= PROBLEM_PROTOCOL:
        parts.append(f'обрывы: {net.get("problem", "?")}')
    summary = ', '.join(parts)
    logger.info(f'стенд: METF {host} - есть, {summary}')

    # Сеть из прошивки плата берёт, только пока не запомнила другую. Запомненная
    # переживает перезагрузку, и после прерванного прогона плата вернётся не
    # туда, где её ищет stand.ini
    if net.get('source') == 'saved':
        logger.warning(
            f'METF: сеть «{ssid}» запомнена порталом платы и перекрывает '
            f'зашитую при сборке. Снять: curl -d action=forget http://{host}/wifi')
    if isinstance(rssi, int) and rssi <= WEAK_RSSI:
        logger.warning(
            f'METF: запас сигнала {rssi} дБм - при таком плата отваливается на '
            f'сетевых тестах и уносит с собой весь хвост прогона')
    if net.get('ap_up'):
        logger.warning(
            'METF: поднята своя точка доступа - плата недавно теряла сеть '
            f'(причина: {net.get("problem", "?")})')
    if net.get('hw_error'):
        logger.warning('METF: плата сообщает об отказе железа (hw_error)')

    home = net.get('channel')
    if stand_channel and isinstance(home, int) and 0 < abs(home - stand_channel) < CHANNEL_GAP:
        logger.warning(
            f'эфир: точка стенда на канале {stand_channel}, домашняя сеть - на '
            f'{home}. Каналы перекрываются, и это хуже общего канала: платы не '
            f'слышат друг друга и передают одновременно. Отсюда потерянные '
            f'сегменты, оборванные тела посылок и провалы чтения лога. '
            f'Разведите каналы (1, 6, 11) или уведите Мак в провод')
    return summary
