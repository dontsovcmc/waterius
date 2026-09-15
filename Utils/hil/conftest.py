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

import time
from pathlib import Path
from typing import TYPE_CHECKING, Any, Iterator

import pytest
from loguru import logger

if TYPE_CHECKING:                       # только для подсказок типов
    from .broker import MqttBroker
    from .mqttwatch import MqttWatch
    from .router import RouterState
    from .stand import Stand

# Модули стенда тянут pyserial и paho-mqtt, а разбор лога проверяется без
# железа и без этих зависимостей. Поэтому импорт - внутри фикстур.


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption('--stand', action='store_true', default=False,
                     help='гонять тесты на собранном стенде')
    parser.addoption('--stand-config', default=None,
                     help='путь к stand.ini')
    parser.addoption('--pcap', action='store_true', default=False,
                     help='снимать дамп трафика точки доступа к упавшим тестам')
    parser.addoption('--experimental', action='store_true', default=False,
                     help='гонять тесты экспериментальных функций прошивки')
    parser.addoption('--soak', action='store_true', default=False,
                     help='гонять многочасовой прогон (test_soak.py)')
    parser.addoption('--soak-minutes', type=int, default=720,
                     help='длительность многочасового прогона, минут')


def pytest_configure(config: pytest.Config) -> None:
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

    if config.getoption('--stand'):
        return
    skip = pytest.mark.skip(reason='нужен стенд: pytest --stand')
    for item in items:
        if 'stand' in item.keywords:
            item.add_marker(skip)


@pytest.fixture(scope='session')
def cfg(request: pytest.FixtureRequest) -> Any:
    from . import config as stand_config
    return stand_config.load(request.config.getoption('--stand-config'))


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


@pytest.fixture(scope='session')
def stand(cfg: Any, mqtt: Any) -> Iterator[Any]:
    from .stand import Stand
    device = Stand.create(cfg, mqtt)    # METF и роутер: без них дальше нечем
    device.check_atboard()     # до первого теста, а не на сороковой минуте
    device.identify()          # версии и MAC - у самого устройства, до первого теста
    device.ensure_network()    # и сеть: в чужой стенд бесполезен
    device.ensure_mqtt()       # и брокер, если он поднялся
    device.ensure_clock()      # и время: иначе оно приходит из интернета
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
        device.dut.init()


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
    if need_esp is not None:
        if stand.esp_version is None or stand.esp_version < _version(need_esp):
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
    if ('stand' not in request.keywords or 'portal' in request.keywords
            or not request.config.getoption('--stand')):
        return
    request.getfixturevalue('firmware_versions')
    if 'reset' in request.keywords:
        # Блок R проверяет сами умолчания: выставить требования - значит
        # стереть предмет проверки. Возврат стенда делает фикстура модуля.
        return
    request.getfixturevalue('stand').ensure_requirements(needs(request.node))


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
    yield


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


# Время по ходу прогона. --durations печатает сводку только в конце и по фазам,
# а по многочасовому прогону надо видеть сразу, какой тест и какой файл съедают
# время. Подготовка и возврат стенда входят в сумму: это тоже время теста.
_test_seconds: dict[str, float] = {}
_file_seconds: dict[Path, float] = {}


def elapsed(seconds: float) -> str:
    minutes, sec = divmod(round(seconds), 60)
    hours, minutes = divmod(minutes, 60)
    return f'{hours:02d}:{minutes:02d}:{sec:02d}'


def pytest_runtest_logreport(report: pytest.TestReport) -> None:
    _test_seconds[report.nodeid] = _test_seconds.get(report.nodeid, 0.0) + report.duration


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_protocol(item: pytest.Item, nextitem: pytest.Item | None):
    """После теста - его время, после последнего теста файла - время файла."""
    yield
    spent = _test_seconds.pop(item.nodeid, 0.0)
    _file_seconds[item.path] = _file_seconds.get(item.path, 0.0) + spent

    terminal = item.config.pluginmanager.get_plugin('terminalreporter')
    if terminal is None:
        return
    terminal.write_line(f'    время теста: {elapsed(spent)}')
    # Причину - сразу: сводка pytest печатает её в конце многочасового прогона
    for when in ('setup', 'call', 'teardown'):
        report = getattr(item, f'rep_{when}', None)
        if report is not None and report.failed:
            terminal.write_line(f'--- причина падения ({when}) ---')
            terminal.write_line(report.longreprtext)
    if nextitem is None or nextitem.path != item.path:
        terminal.write_line(
            f'--- {item.path.name}: {elapsed(_file_seconds[item.path])} ---')
