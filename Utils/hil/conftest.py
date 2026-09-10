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
        'arm(**settings): доп. настройки для фикстуры тревог; иначе тесту '
        'пришлось бы платить за второй сеанс на живом железе')
    config.addinivalue_line(
        'markers',
        'requires(attiny=N, esp="X.Y.Z"): минимальные версии прошивки для теста; '
        'без указания версии тест идёт на любой')


def pytest_collection_modifyitems(config: pytest.Config,
                                  items: list[pytest.Item]) -> None:
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
    device = Stand.create(cfg, mqtt)
    logger.info(f'роутер: {device.router.version()}')
    device.identify()          # версии и MAC - у самого устройства, до первого теста
    device.ensure_network()    # и сеть: в чужой стенд бесполезен
    device.ensure_mqtt()       # и брокер, если он поднялся
    try:
        yield device
    finally:
        device.close()


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


@pytest.fixture(autouse=True)
def device_baseline(request: pytest.FixtureRequest) -> None:
    """
    Известное состояние устройства перед каждым тестом стенда.

    Иначе тест наследует настройки соседа: test_I5 оставляет выключенной
    квитанцию MQTT, а test_G1 ничего не настраивает и требует её включённой -
    при полном прогоне он падает и обвиняет прошивку.

    Тесты портала исключены: устройство там в режиме настройки, обычного
    сеанса с посылкой не будет, и приведение к эталону просто не дождётся его.
    """
    if ('stand' not in request.keywords or 'portal' in request.keywords
            or not request.config.getoption('--stand')):
        return
    request.getfixturevalue('firmware_versions')
    if 'reset' in request.keywords:
        # Блок R проверяет сами умолчания: выставить эталон - значит стереть
        # предмет проверки. Возврат стенда делает фикстура модуля.
        return
    request.getfixturevalue('stand').ensure_baseline()


@pytest.fixture
def discovery_on(request: pytest.FixtureRequest) -> None:
    """
    Предусловие тестов команд: включённое автодискавери.

    Подписку на `<топик>/#` и сам обработчик команд прошивка заводит только при
    нём (`senders/sender_mqtt.h`), поэтому без него команда из Home Assistant не
    доедет вовсе - и тест обвинит устройство в том, чего оно не обещало.

    Состояние читаем в посылке (`ha` - это mqtt плюс автодискавери,
    `core/routing.cpp`), чтобы не платить сеансом там, где всё и так включено.
    """
    if not request.config.getoption('--stand'):
        return
    stand = request.getfixturevalue('stand')
    if not (stand.last_payload or {}).get('ha'):
        stand.setup(mqtt_auto_discovery=1)


@pytest.fixture
def discovery_off(request: pytest.FixtureRequest) -> None:
    """
    Обратное предусловие: автодискавери выключено.

    Тогда показания уходят по топику на поле, а команды не доезжают вовсе -
    подписки у прошивки нет. Это отдельный режим работы, и проверять его надо
    отдельными тестами, а не полагаться на то, что осталось от соседа.
    """
    if not request.config.getoption('--stand'):
        return
    stand = request.getfixturevalue('stand')
    if (stand.last_payload or {}).get('ha'):
        stand.setup(mqtt_auto_discovery=0)


@pytest.fixture(scope='module')
def discovery_reset(request: pytest.FixtureRequest) -> Iterator[None]:
    """
    Выключить автодискавери, когда группа MQTT отработала.

    Публикация автодискавери - это две с половиной сотни строк лога за сеанс, а
    кольцо METF держит около тридцати пяти и вытесняет старые молча. Оставленное
    включённым, оно уносит из лога `Startup mode:` следующих тестов, и те ждут
    свой сеанс до таймаута, обвиняя устройство.
    """
    yield
    if not request.config.getoption('--stand'):
        return
    if request.getfixturevalue('broker') is None:
        return
    request.getfixturevalue('stand').setup(mqtt_auto_discovery=0)


@pytest.fixture
def quiet(stand: Any, device_baseline: None) -> Iterator[None]:
    """
    Для тестов тревог: начинать с состояния, в котором устройство никого не
    будит. Остаток бюджета внеплановых сеансов от предыдущего теста иначе
    утечёт в этот и собьёт счёт.
    """
    stand.wait_quiet(seconds=330)
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
