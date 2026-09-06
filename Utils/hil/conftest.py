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
    from .broker import Mosquitto
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


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line('markers', 'stand: требует собранного стенда')
    config.addinivalue_line('markers', 'slow: идёт десятки минут')


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
    from .broker import Mosquitto
    if not Mosquitto.available():
        pytest.skip('mosquitto не установлен: brew install mosquitto')
    server = Mosquitto(cfg.broker_port)
    server.start()
    try:
        yield server
    finally:
        server.stop()


@pytest.fixture(scope='session')
def mqtt(cfg: Any, broker: Any) -> Iterator[Any]:
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
    if 'stand' not in request.keywords or not request.config.getoption('--stand'):
        yield
        return

    device = request.getfixturevalue('stand')
    state = request.getfixturevalue('baseline')
    device.reset_observers()
    try:
        yield
    finally:
        device.router.restore(state)


@pytest.fixture
def quiet(stand: Any) -> Iterator[None]:
    """
    Для тестов тревог: начинать с состояния, в котором устройство никого не
    будит. Остаток бюджета внеплановых сеансов от предыдущего теста иначе
    утечёт в этот и собьёт счёт.
    """
    stand.wait_quiet(seconds=330)
    yield


@pytest.fixture
def capture(request: pytest.FixtureRequest, stand: Any,
            tmp_path_factory: pytest.TempPathFactory) -> Iterator[None]:
    """Дамп трафика к упавшему тесту: по логу видно намерение, по дампу - факт."""
    path = tmp_path_factory.mktemp('pcap') / f'{request.node.name}.pcap'
    host = stand.cfg.router_host or stand.cfg.dut_ip.rsplit('.', 1)[0] + '.1'
    with stand.router.capture(host, str(path)):
        yield
    if path.exists() and path.stat().st_size:
        logger.info(f'дамп трафика: {path}')


@pytest.fixture
def slow_clock() -> Iterator[None]:
    """Отметка в логе, чтобы в отчёте было видно, сколько шёл долгий тест."""
    start = time.time()
    yield
    logger.info(f'тест занял {time.time() - start:.0f} с')
