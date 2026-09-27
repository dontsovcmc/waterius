"""
Фикстуры стенда.

Тесты делятся на два вида. Разбор лога проверяется без железа и гоняется где
угодно. Всё остальное помечено `stand` и требует собранного стенда, поэтому по
умолчанию пропускается: `pytest --stand` включает.

Главная фикстура - не та, что поднимает стенд, а та, что возвращает его в
исходное состояние. Тест, упавший с выключенной точкой доступа или с правилом
фильтра, иначе уронит весь прогон: Ватериус просто не найдёт сеть.
"""

from __future__ import annotations

import logging
import time
from collections.abc import Callable, Iterator
from pathlib import Path
from typing import TYPE_CHECKING, Any

import pytest
from loguru import logger

from .hostclock import host

if TYPE_CHECKING:                       # только для подсказок типов
    pass

# Модули стенда тянут pyserial и paho-mqtt, а разбор лога проверяется без
# железа и без этих зависимостей. Поэтому импорт - внутри фикстур.


class _Tee:
    """
    Пишет в терминал и в файл сразу.

    Подменяет файл у терминального писателя pytest, поэтому в лог попадает всё,
    что видно на экране: строки тестов, сводка, traceback.
    """

    def __init__(self, stream: Any, log: Any) -> None:
        self._stream = stream
        self._log = log

    def write(self, data: str) -> int:
        self._log.write(data)
        return self._stream.write(data)

    def flush(self) -> None:
        self._log.flush()
        self._stream.flush()

    def isatty(self) -> bool:
        return bool(self._stream.isatty())

    def __getattr__(self, name: str) -> Any:
        return getattr(self._stream, name)


def _line(record: Any) -> str:
    """
    Строка лога прогона. Записи селф-тестов помечены: они нарочно гоняют стенд
    по путям отказа, и без пометки их предупреждения читаются как настоящие
    поломки - две трети всех WARNING полного прогона приходили оттуда.
    """
    метка = 'подделка | ' if record['extra'].get('поддельно') else ''
    return '{time:HH:mm:ss} | {level: <7} | ' + метка + '{message}\n'


def _next_log(where: Path) -> Path:
    """
    Имя лога прогона: `logs/ГГГГ-мм-ДД-ЧЧ-ММ.log` в корне репозитория.

    Минуты хватает, чтобы найти прогон по времени, но два прогона в одну минуту
    затёрли бы друг друга - поэтому второму добавляется номер.
    """
    stem = time.strftime('%Y-%m-%d-%H-%M')
    path = where / f'{stem}.log'
    n = 2
    while path.exists():
        path = where / f'{stem}-{n}.log'
        n += 1
    return path


def _open_log(config: pytest.Config) -> Path | None:
    """
    Завести лог прогона. По умолчанию - всегда, без флагов и без `tee`.

    Прогон стенда идёт часами, и единственный способ понять, что он делает
    сейчас и на чём умер, - лог на диске. Пишется построчно, поэтому оборванный
    прогон сохраняет всё, что успел напечатать. Три источника в одном файле:
    вывод pytest, loguru (сам стенд) и стандартный logging (клиент METF).
    """
    where = config.getoption('--stand-log')
    if where == 'off':
        return None

    path = Path(where) if where else _next_log(config.rootpath / 'logs')
    path.parent.mkdir(parents=True, exist_ok=True)
    log = path.open('w', buffering=1, encoding='utf-8')
    config.add_cleanup(log.close)

    reporter = config.pluginmanager.get_plugin('terminalreporter')
    if reporter is not None:
        reporter._tw._file = _Tee(reporter._tw._file, log)

    sink = logger.add(log, level='INFO', colorize=False, format=_line)
    config.add_cleanup(lambda: logger.remove(sink))

    handler = logging.StreamHandler(log)
    handler.setFormatter(logging.Formatter('%(asctime)s | %(levelname)-7s | %(message)s',
                                           datefmt='%H:%M:%S'))
    logging.getLogger().addHandler(handler)
    config.add_cleanup(lambda: logging.getLogger().removeHandler(handler))
    return path


def pytest_report_header(config: pytest.Config) -> str | None:
    path = getattr(config, '_stand_log', None)
    return f'лог прогона: {path}' if path else None


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption('--stand', action='store_true', default=False,
                     help='гонять тесты на собранном стенде')
    parser.addoption('--stand-config', default=None,
                     help='путь к stand.ini')
    parser.addoption('--stand-log', default=None,
                     help='файл лога прогона; по умолчанию logs/ГГГГ-мм-ДД-ЧЧ-ММ.log '
                          'в корне репозитория, "off" - не писать')
    parser.addoption('--ap-channel', type=int, default=None,
                     help='канал точки стенда; по умолчанию из stand.ini. Эфир решает '
                          'судьбу прогона: канал держат дальше пяти от домашней сети')
    parser.addoption('--ap-channel-other', type=int, default=None,
                     help='запасной канал для тестов смены канала (W4, W5)')
    parser.addoption('--ap-bandwidth', type=int, default=None,
                     help='полоса, которую точка обязана вещать, МГц: стенд сверяет '
                          'её со сканом эфира и говорит о расхождении')
    parser.addoption('--pcap', action='store_true', default=False,
                     help='снимать дамп трафика точки доступа к упавшим тестам')
    parser.addoption('--experimental', action='store_true', default=False,
                     help='гонять тесты экспериментальных функций прошивки')
    parser.addoption('--soak', action='store_true', default=False,
                     help='гонять длинный прогон (test_soak.py)')
    parser.addoption('--reverse', action='store_true', default=False,
                     help='гонять набор задом наперёд: так ловится зависимость '
                          'по порядку, которая в прямом прогоне не видна')
    parser.addoption('--soak-minutes', type=int, default=60,
                     help='длительность длинного прогона, минут: по умолчанию 60 - это '
                          '12 сеансов, чтобы набор укладывался в один заход; '
                          'для редких суточных дефектов ставят сотни')


@pytest.hookimpl(trylast=True)               # терминальный репортер создаётся в своём
def pytest_configure(config: pytest.Config) -> None:   # pytest_configure - ждём его
    config._stand_log = _open_log(config)       # type: ignore[attr-defined]
    if config.getoption('--stand'):
        # Прогон занимает железо часами, а хост засыпает через минуту простоя.
        # Держим удержание сами, а не флагом в командной строке: флаг забывают,
        # и цена забывчивости - прогон, в котором девять падений из десяти про сон
        from .hostclock import keep_awake
        awake = keep_awake()
        if awake is not None:
            config.add_cleanup(awake.terminate)
    config.addinivalue_line('markers', 'stand: требует собранного стенда')
    config.addinivalue_line('markers', 'slow: идёт десятки минут')
    config.addinivalue_line('markers', 'mqtt: нужен брокер (amqtt из requirements.txt)')
    config.addinivalue_line('markers', 'portal: режим настройки и AT-плата')
    config.addinivalue_line(
        'markers',
        'reset: заводской сброс; эталон настроек тесту не выставляют - он его '
        'и проверяет, а возврат стенда делает сам модуль')
    config.addinivalue_line(
        'markers',
        'requires(attiny=N, esp="X.Y.Z"): минимальные версии прошивки для теста; '
        'без указания версии тест идёт на любой')
    config.addinivalue_line(
        'markers',
        'needs(**settings): настройки устройства, которые нужны тесту поверх '
        'общих (Stand.requirements); имена - параметров прошивки. Ближний '
        'маркер перебивает дальний: тест - модуль')
    config.addinivalue_line(
        'markers',
        'experimental: функция прошивки ещё не устоялась, поведение может '
        'измениться; по умолчанию пропускается, гоняется с --experimental')
    config.addinivalue_line(
        'markers',
        'soak: многочасовой прогон; занимает стенд целиком, поэтому только с --soak')


@pytest.hookimpl(tryfirst=True)
def pytest_exception_interact(node: Any, call: Any, report: Any) -> None:
    """
    METF замолчал надолго или простоял хост - останавливаем прогон целиком.

    Без платы стенд не может ни нажать кнопку, ни прочитать лог, поэтому
    продолжать бессмысленно: каждый следующий тест выдаст тот же traceback.
    Один раз это стоило часа прогона и 23 одинаковых ошибок подряд.

    Простой хоста хуже: он не виден вовсе. Пока Мак спал, лог никто не
    вычитывал, кольцо METF переполнялось, и обстановка устройства неизвестна -
    дальнейшие вердикты не стоят ничего. Прогон 2026-09-27 дал так девять
    ложных падений из десяти.
    """
    from .hostclock import HostAsleep
    from .metf import MetfGone

    if call.excinfo is None:
        return
    if isinstance(call.excinfo.value, (MetfGone, HostAsleep)):
        pytest.exit(str(call.excinfo.value), returncode=1)


def pytest_collection_modifyitems(config: pytest.Config,
                                  items: list[pytest.Item]) -> None:
    if not config.getoption('--experimental'):
        skip = pytest.mark.skip(
            reason='экспериментальная функция: pytest --experimental')
        for item in items:
            if 'experimental' in item.keywords:
                item.add_marker(skip)

    if not config.getoption('--soak'):
        skip = pytest.mark.skip(reason='многочасовой прогон: pytest --soak')
        for item in items:
            if 'soak' in item.keywords:
                item.add_marker(skip)

    if config.getoption('--reverse'):
        # Зависимость по порядку - каждый восьмой нестабильный тест в природе,
        # и в прямом прогоне она сидит в зелёном наборе до любой перестановки.
        items.reverse()

    if config.getoption('--stand'):
        return
    skip = pytest.mark.skip(reason='нужен стенд: pytest --stand')
    for item in items:
        if 'stand' in item.keywords:
            item.add_marker(skip)


@pytest.fixture(scope='session')
def cfg(request: pytest.FixtureRequest) -> Any:
    """Настройки стенда; каналы и полосу можно перебить аргументом прогона."""
    import dataclasses

    from . import config as stand_config
    loaded = stand_config.load(request.config.getoption('--stand-config'))
    asked = {name: request.config.getoption(f'--{name.replace("_", "-")}')
             for name in ('ap_channel', 'ap_channel_other', 'ap_bandwidth')}
    given = {name: value for name, value in asked.items() if value is not None}
    return dataclasses.replace(loaded, **given) if given else loaded


@pytest.fixture(scope='session')
def broker(cfg: Any) -> Iterator[Any]:
    """Свой брокер: тесты retain и автодискавери должны начинаться с чистых топиков."""
    from .broker import MqttBroker
    if not MqttBroker.available():
        # Не пропуск: от брокера зависят только тесты с меткой mqtt, а пропуск
        # здесь увёл бы в пропуск весь стенд - фикстура stand стоит на этой же
        # цепочке.
        logger.warning('нет amqtt: тесты MQTT будут пропущены')
        yield None
        return
    server = MqttBroker(cfg.broker_port, cfg.broker_host)
    server.start()
    try:
        yield server
    finally:
        server.stop()


@pytest.fixture(scope='session')
def mqtt(cfg: Any, broker: Any) -> Iterator[Any]:
    if broker is None:
        yield None
        return
    from .mqttwatch import MqttWatch
    watch = MqttWatch(cfg.broker_host, cfg.broker_port, cfg.mqtt_topic)
    try:
        yield watch
    finally:
        watch.close()


def bring_up(step: Callable[[], Any], what: str) -> Any:
    """
    Шаг подъёма стенда, переживающий одну осечку связи с METF.

    Это не обезболивающее для мигающих тестов, а разница в цене: шаги подъёма
    ничего не утверждают о прошивке и повторяются без последствий, а их отказ
    уносит весь прогон целиком. Так и вышло: отлучка платы на 1,1 с переполнила
    кольцо лога, и 45 медленных тестов не начались вовсе.

    Повторяется только то, что случилось со стендом: переполнение кольца, слепота,
    когда METF не отдала ни одного чтения, и сеанс, целое тело которого не
    доехало - о состоянии устройства такой сеанс не говорит ничего, это его
    собственные слова. Отказ по делу - устройство молчит при живой связи, сеть
    чужая - летит сразу, как раньше.
    """
    try:
        return step()
    except AssertionError as err:
        beda = ('METF потерял', 'стенд ослеп', 'не говорит ничего')
        if not any(mark in str(err) for mark in beda):
            raise
        logger.warning(f'подъём стенда: {what} сорвался на беде стенда, повторяю\n{err}')
        return step()


def preflight(step: Callable[[], Any], what: str) -> Any:
    """
    Шаг подъёма, чей отказ уносит прогон целиком.

    Отказ сессионной фикстуры pytest запоминает и повторяет в каждом тесте:
    прогон печатает по одинаковой ошибке на тест и заканчивается только на
    последнем. Один раз это дало 91 одинаковый ERROR - attiny была прошита
    окружением другой модели, и ЕСП её не нашла.
    """
    try:
        return bring_up(step, what)
    except AssertionError as err:
        pytest.exit(f'стенд не поднялся ({what}): {err}', returncode=1)


@pytest.fixture(scope='session')
def stand(cfg: Any, mqtt: Any) -> Iterator[Any]:
    from .stand import Stand
    device = Stand.create(cfg, mqtt)    # METF и роутер: без них дальше нечем
    global _metf
    _metf = device.api
    global _stand
    _stand = device
    preflight(device.check_atboard, 'AT-плата')      # до первого теста, а не на сороковой минуте
    preflight(device.identify, 'опрос устройства')   # версии и MAC - до первого теста
    preflight(device.ensure_network, 'сеть стенда')  # в чужой сети стенд бесполезен
    preflight(device.ensure_mqtt, 'брокер стенда')   # если он поднялся
    preflight(device.ensure_clock, 'часы платы')     # иначе время придёт из интернета
    _log_air('первый тест')   # у первого файла улик иначе нет: стенд встал позже хука
    try:
        yield device
    finally:
        device.close()


@pytest.fixture(autouse=True)
def armed_clock(request: pytest.FixtureRequest) -> None:
    """
    Часы платы на месте перед каждым тестом стенда.

    Плата теряет назначенное время при перезагрузке, и сервер после этого
    молча выбрасывает запросы - устройство осталось бы без времени, а тест
    падал бы совсем в другом месте. Проверка стоит одного HTTP-запроса.
    """
    if 'stand' not in request.fixturenames:
        return
    if not request.config.getoption('--stand'):
        return
    request.getfixturevalue('stand').arm_clock()


@pytest.fixture(scope='session')
def baseline(stand: Any) -> Any:
    """Снимок настроек роутера, к которому возвращаемся после каждого теста."""
    return stand.router.snapshot()


@pytest.fixture(autouse=True)
def clean_net(request: pytest.FixtureRequest) -> Iterator[None]:
    """
    Возврат сети в исходное состояние после каждого теста стенда.

    Именно после, а не до: упавший тест обязан оставить стенд рабочим, иначе
    следующий упадёт по чужой причине и разбираться придётся с конца.
    """
    if ('stand' not in request.keywords or 'portal' in request.keywords
            or not request.config.getoption('--stand')):
        yield
        return

    device = request.getfixturevalue('stand')
    state = request.getfixturevalue('baseline')
    device.reset_observers()
    try:
        yield
    finally:
        device.router.restore(state)


@pytest.fixture(autouse=True)
def clean_dut(request: pytest.FixtureRequest) -> Iterator[None]:
    """
    Возврат линий METF в высокоомное состояние после каждого теста стенда.

    Стенд держит линии, пока их не отпустят: `dut.wet(closed=True)` - это
    pinMode(OUTPUT) плюс digitalWrite(LOW) до отдельной команды. Тест, упавший
    между замыканием входа и своим `finally`, оставляет вход прижатым к земле
    на весь оставшийся прогон, и следующий тест считает чужие импульсы или
    видит вечную тревогу датчика.

    После, а не до, по той же причине, что и у clean_net: упавший тест обязан
    оставить стенд рабочим. Начало прогона закрыто отдельно - `Stand.create()`
    зовёт `dut.init()` при подъёме сессии, так что перед первым тестом линии
    тоже свободны.

    Тесты портала не исключены, в отличие от clean_net: мастер и плашки тоже
    подают импульсы (W1, B3, B4), а отпущенная линия режиму настройки не мешает.
    """
    if 'stand' not in request.keywords or not request.config.getoption('--stand'):
        yield
        return

    device = request.getfixturevalue('stand')
    try:
        yield
    finally:
        device.release_lines()


# Тесты, в которых плата-манипулятор перезагружалась: имя -> сколько раз. Нужны
# и сразу (припиской к отказу), и в конце прогона сводкой: перезагрузка METF
# объясняет пустое кольцо, выключенный сервер времени и отпущенные выводы, и
# упавший рядом с ней тест - повод смотреть на стенд, а не на прошивку.
_reboots: dict[str, int] = {}

# Клиент METF, пока живёт стенд. Счётчик перезагрузок снимается хуком вокруг
# всего теста, а не фикстурой: тест, упавший в подготовке, до фикстуры не
# доходит - а именно в подготовке перезагрузка и мешает чаще всего.
_metf: Any = None

# Сам стенд, пока он жив: нужен для улик про эфир перед каждым файлом тестов.
_stand: Any = None

# Файл, перед которым улики уже сняты: снимаем один раз на файл, а не на тест -
# опрос METF идёт по тому же радио, о котором мы и собираем улики.
_air_at: Any = None


def _log_air(where: str) -> None:
    """
    Записать, кого и как слышно, перед файлом тестов.

    Отказ обязан приносить улику, а главная улика стенда - уровень сигнала:
    оборванные тела посылок и провалы чтения лога рождаются в эфире и выглядят
    дефектом прошивки (05_air-and-loss.md). Задним числом эти числа взять
    неоткуда, поэтому они пишутся всегда, а не только к падению.
    """
    if _stand is None:
        return
    parts = []
    try:
        net = _stand.api.wifi()
        parts.append(f'METF {net.get("rssi")} дБм (сеть «{net.get("ssid")}», '
                     f'канал {net.get("channel")})')
    except Exception as err:                 # опрос по радио: сам может не дойти
        parts.append(f'METF о себе не сказала ({type(err).__name__})')
    payload = _stand.last_payload or {}
    if 'rssi' in payload:
        parts.append(f'Ватериус {payload["rssi"]} дБм на канале '
                     f'{payload.get("channel")}, ошибок связи '
                     f'{payload.get("wifi_connect_errors")} - из последней посылки')
    logger.info(f'эфир перед {where}: ' + '; '.join(parts))


def _version(text: str) -> tuple[int, ...]:
    return tuple(int(part) for part in str(text).split('.'))


@pytest.fixture(autouse=True)
def needs_broker(request: pytest.FixtureRequest) -> None:
    """Тест с меткой mqtt без брокера пропускается, остальные идут как обычно."""
    if 'mqtt' not in request.keywords or not request.config.getoption('--stand'):
        return
    if request.getfixturevalue('broker') is None:
        pytest.skip('нужен брокер: pip install -r Utils/hil/requirements.txt')


@pytest.fixture(autouse=True)
def firmware_versions(request: pytest.FixtureRequest) -> None:
    """
    Пропустить тест, если прошивка на стенде младше требуемой.

    Версии берутся у самого устройства (`Stand.identify`), а не из stand.ini:
    прошивку на стенде меняют чаще, чем правят конфиг, и рассинхрон означал бы
    красный прогон вместо честного «этой версии тест не про неё». Тест без
    маркера идёт на любой версии.
    """
    marker = request.node.get_closest_marker('requires')
    if marker is None or 'stand' not in request.keywords:
        return
    if not request.config.getoption('--stand'):
        return

    stand = request.getfixturevalue('stand')
    need_attiny = marker.kwargs.get('attiny')
    if need_attiny is not None and (stand.attiny_version or 0) < need_attiny:
        pytest.skip(f'нужна attiny {need_attiny}, на стенде {stand.attiny_version}')

    need_esp = marker.kwargs.get('esp')
    if need_esp is not None and (stand.esp_version is None
                                 or stand.esp_version < _version(need_esp)):
        pytest.skip(f'нужна ЕСП {need_esp}, на стенде {stand.version_str}')


def needs(node: pytest.Item) -> dict[str, Any]:
    """Настройки из маркеров needs: модуля, потом теста - ближний перебивает."""
    out: dict[str, Any] = {}
    for marker in reversed(list(node.iter_markers('needs'))):
        out.update(marker.kwargs)
    return out


@pytest.fixture(autouse=True)
def device_baseline(request: pytest.FixtureRequest) -> None:
    """
    Устройство отвечает требованиям теста - общим и своим из маркера needs.

    Иначе тест наследует настройки соседа: test_I5 оставляет выключенной
    квитанцию MQTT, а test_G1 ничего не настраивает и требует её включённой -
    при полном прогоне он падает и обвиняет прошивку. Перенастройка - только
    при расхождении с последним известным состоянием (Stand.ensure_requirements).

    Тесты портала исключены: устройство там в режиме настройки, обычного
    сеанса с посылкой не будет, и перенастройка просто не дождётся его.
    """
    if 'stand' not in request.keywords or not request.config.getoption('--stand'):
        return
    if 'portal' in request.keywords:
        # Базу тесту портала ставить нечем, а своё требование он обязан
        # получить: иначе маркер обещает предусловие и молча его не даёт -
        # test_E19 просил датчик протечки на входе 0, получал механический
        # счётчик и семь минут ждал тревогу, которой неоткуда взяться
        свои = needs(request.node)
        if свои:
            request.getfixturevalue('firmware_versions')
            стенд = request.getfixturevalue('stand')
            bring_up(lambda: стенд.ensure_requirements(свои, only=True),
                     'требования теста')
        return
    request.getfixturevalue('firmware_versions')
    if 'reset' in request.keywords:
        # Блок R проверяет сами умолчания: выставить требования - значит
        # стереть предмет проверки. Возврат стенда делает фикстура модуля.
        return
    стенд = request.getfixturevalue('stand')
    # Осечка стенда в фикстуре приходит как ERROR, а не FAILED, и читается как
    # «тест даже не начался по вине прошивки». Повтор для беды стенда уже
    # написан для подъёма - здесь он нужен ровно затем же
    bring_up(lambda: стенд.ensure_requirements(needs(request.node)),
             'требования теста')


@pytest.fixture(autouse=True)
def forget_state(request: pytest.FixtureRequest) -> Iterator[None]:
    """
    После теста, менявшего настройки мимо стенда, состояние устройства неизвестно.

    Портал и заводской сброс меняют настройки без сеанса с посылкой, а упавший
    тест мог оборваться между настройкой и её проверкой. Следующий тест тогда
    начнёт с короткого нажатия, а не поверит устаревшему состоянию.
    """
    if 'stand' not in request.keywords or not request.config.getoption('--stand'):
        yield
        return

    stand = request.getfixturevalue('stand')
    yield
    touched = 'portal' in request.keywords or 'reset' in request.keywords
    failed = any(getattr(request.node, f'rep_{when}', None) is not None
                 and getattr(request.node, f'rep_{when}').failed
                 for when in ('setup', 'call'))
    if touched or failed:
        stand.forget_state()


@pytest.fixture
def fresh_device(cfg: Any, stand: Any) -> Iterator[Any]:
    """
    Устройство сразу после заводского сброса, в своём портале (`reset.py`).

    Сброс на каждый тест: вес, заданный одним тестом, другому уже не снять, а
    незаданным он бывает только после сброса. Тестам с этой фикстурой нужна
    метка `reset` - эталон перед ними выставлять незачем, его сотрёт сброс.
    Стенд после теста возвращается в рабочее состояние сам.
    """
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    from .reset import FreshDevice
    device = FreshDevice.reset(cfg, stand)
    try:
        yield device
    finally:
        device.close()


@pytest.fixture
def quiet(stand: Any, device_baseline: None) -> Iterator[None]:
    """
    Для тестов тревог: начинать с заведомо снятыми тревогами.

    Тревога сама не гаснет, так что чужая, доставшаяся от предыдущего теста,
    висела бы до конца прогона и ломала каждый следующий: устройство уже
    доложило о ней и внепланового сеанса больше не даст.
    """
    stand.assert_no_alarms()
    return


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo[None]):
    """Итог теста - фикстурам: дамп трафика нужен только от упавшего."""
    outcome = yield
    setattr(item, f'rep_{call.when}', outcome.get_result())


@pytest.fixture(autouse=True)
def capture(request: pytest.FixtureRequest,
            tmp_path_factory: pytest.TempPathFactory) -> Iterator[None]:
    """
    Дамп трафика точки доступа: `pytest --stand --pcap`.

    По логу видно, что прошивка думала, по дампу - что ушло в эфир. Дамп
    зелёного теста удаляется: смысл он имеет только рядом с падением, а
    круглосуточный прогон иначе засыпает диск.
    """
    if not (request.config.getoption('--pcap') and 'stand' in request.keywords):
        yield
        return

    stand = request.getfixturevalue('stand')
    path = tmp_path_factory.mktemp('pcap') / f'{request.node.name}.pcap'
    host = stand.cfg.router_host or stand.cfg.dut_ip.rsplit('.', 1)[0] + '.1'
    with stand.router.capture(host, str(path)):
        yield

    if not path.exists() or not path.stat().st_size:
        return
    failed = any(getattr(request.node, f'rep_{when}', None) is not None
                 and getattr(request.node, f'rep_{when}').failed
                 for when in ('setup', 'call'))
    if failed:
        logger.info(f'дамп трафика: {path}')
    else:
        path.unlink()


def _metf_note() -> str:
    """
    Время METF от её перегрузки - в каждый отказ.

    По нему видно, падала ли плата: вся картина о Ватериусе приходит через неё,
    и перезагрузившаяся посреди теста плата объясняет и пустой лог, и потерянный
    импульс. Спрашивать плату не нужно, аптайм приезжает заголовком
    `X-Uptime-Ms` на каждом ответе и лежит в `Metf.uptime_ms`.
    """
    if _metf is None or _metf.uptime_ms is None:
        return 'METF: аптайм неизвестен (плата ни разу не ответила)'
    return (f'METF: от перегрузки {_metf.uptime_ms / 1000:.0f} с, '
            f'перезагрузок за прогон {_metf.reboots}')


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_protocol(item: pytest.Item, nextitem: pytest.Item | None):
    """
    Вокруг теста - улики стенда: эфир перед новым файлом, перезагрузки платы и
    причина падения сразу.

    Время теста печатает сам pytest (`console_output_style = times` в pytest.ini
    вместе с `-v`), поэтому своего учёта здесь нет: самописная строка вдобавок
    сбрасывала `currentfspath` терминального репортера, и pytest заново печатал
    имя файла перед каждым тестом - из-за этого тесты одного файла в логе не
    отличались.
    """
    global _air_at
    if item.path != _air_at:
        _air_at = item.path
        _log_air(item.path.name)
    before = _metf.reboots if _metf is not None else 0
    yield
    if _metf is not None and _metf.reboots > before:
        _reboots[item.nodeid] = _metf.reboots - before
        logger.warning(f'METF перезагружалась во время теста {item.name}: '
                       f'{_metf.reboots - before} раз')

    terminal = item.config.pluginmanager.get_plugin('terminalreporter')
    if terminal is None:
        return
    # Причину - сразу: сводка pytest печатает её в конце многочасового прогона
    for when in ('setup', 'call', 'teardown'):
        report = getattr(item, f'rep_{when}', None)
        if report is not None and report.failed:
            terminal.write_line(f'--- причина падения ({when}) ---')
            terminal.write_line(report.longreprtext)
            terminal.write_line(f'--- {_metf_note()}{host.note()} ---')
            if item.nodeid in _reboots:
                terminal.write_line(
                    f'--- во время теста METF перезагружалась '
                    f'({_reboots[item.nodeid]} раз): отказ может быть про стенд, '
                    f'а не про Ватериус ---')
    if nextitem is None and _reboots:
        terminal.write_line('--- METF перезагружалась в тестах ---')
        for nodeid, times in _reboots.items():
            terminal.write_line(f'    {nodeid}: {times}')
