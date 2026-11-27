"""
Подключилось устройство к сети или нет - вопрос, на который отвечает его лог.

29 сентября стенд этот ответ не читал. Устройство было настроено на точку
стенда, не подключилось к ней ни разу (`WIFI: Connection failed.` дважды) и
ушло спать, а подъём сверял строки конфига, видел совпадение и говорил
«устройство уже в сети стенда». Прогон умирал минутой позже на пустом приёмнике
отказом про настройки MQTT, которые «до прошивки не доехали».
"""

from __future__ import annotations

from typing import Any

import pytest

from ..logwatch import LogWatcher
from ..stand import Stand
from .test_logwatch import FakeApi, fw, through_ring

ПОДКЛЮЧИЛСЯ = [
    fw('Startup mode: 3'),
    fw('WIFI: Attempt #1 from 2'),
    fw('WIFI: Connected.'),
    fw('HTTP: Response code: 200'),
    fw('Going to sleep'),
]

НЕ_ПОДКЛЮЧИЛСЯ = [
    fw('Startup mode: 3'),
    fw('wifi_ssid=waterius_stand'),
    fw('WIFI: Attempt #1 from 2'),
    fw('WIFI: Connection failed.', 'ERROR'),
    fw('WIFI: Attempt #2 from 2'),
    fw('WIFI: Connection failed.', 'ERROR'),
    fw('Going to sleep'),
]


РАДИО_НЕ_ВКЛЮЧАЛИ = [
    fw('Startup mode: 2'),
    fw('Idle: consumed=0, silence_min=15, transmit=0'),
    fw('Idle: no consumption, WiFi stays off'),
    fw('Going to sleep'),
]


def сеанс(lines: list[str]) -> Any:
    watcher = LogWatcher(FakeApi(through_ring(lines)))
    watcher.poll()
    session = watcher._take_session(None)
    assert session is not None
    return session


class Стенд:
    """Стенд, у которого спрашивают только вердикт о сети."""

    device_config: dict[str, str] = {}
    joined: bool | None = None


def test_подключение_видно_по_строке_прошивки() -> None:
    assert сеанс(ПОДКЛЮЧИЛСЯ).wifi_connected
    assert not сеанс(НЕ_ПОДКЛЮЧИЛСЯ).wifi_connected


def test_попытки_считаются_для_улики() -> None:
    # Одна попытка значит, что больше устройству и не разрешали, две - что сеть
    # не отозвалась ни разу
    assert сеанс(ПОДКЛЮЧИЛСЯ).wifi_attempts == 1
    assert сеанс(НЕ_ПОДКЛЮЧИЛСЯ).wifi_attempts == 2


@pytest.mark.parametrize('lines, ждём', [(ПОДКЛЮЧИЛСЯ, True), (НЕ_ПОДКЛЮЧИЛСЯ, False)])
def test_стенд_запоминает_вердикт(lines: list[str], ждём: bool) -> None:
    стенд = Стенд()
    Stand._note_wifi(стенд, сеанс(lines))
    assert стенд.joined is ждём


def test_сеанс_без_радио_о_сети_не_говорит() -> None:
    """Плановый сеанс без расхода сеть не трогает: прошлый вердикт вернее нового."""
    стенд = Стенд()
    стенд.joined = True
    Stand._note_wifi(стенд, сеанс(РАДИО_НЕ_ВКЛЮЧАЛИ))
    assert стенд.joined is True


class Подъём:
    """Стенд на шаге `ensure_network`: всё остальное подделано."""

    def __init__(self, joined: bool) -> None:
        self.joined = joined
        self.ap_ssid = 'waterius_stand'
        self.device_config = {'wifi_ssid': 'waterius_stand', 'http_on': '1',
                              'http_host': 'http://192.168.100.11:8010/data'}

        class cfg:                   # noqa: N801 - подделка, а не класс конфига
            http_url = 'http://192.168.100.11:8010/data'
            atboard_port = ''        # настраивать нечем: дойдём до отказа и прочтём его

        self.cfg = cfg


class Возврат:
    """Стенд на шаге `rejoin`: сеансы по очереди из списка."""

    def __init__(self, *сеансы: list[str]) -> None:
        self.сеансы = [сеанс(lines) for lines in сеансы]
        self.нажатий = 0
        стенд = self

        class dut:                   # noqa: N801 - подделка платы
            @staticmethod
            def press_button() -> None:
                стенд.нажатий += 1

        self.dut = dut

    def reset_observers(self) -> None:
        pass

    def wait_session(self, timeout: float, mode: int | None = None) -> Any:
        return self.сеансы.pop(0)


def test_возврат_в_сеть_кончается_первым_подключением() -> None:
    стенд = Возврат(НЕ_ПОДКЛЮЧИЛСЯ, ПОДКЛЮЧИЛСЯ, ПОДКЛЮЧИЛСЯ)
    Stand.rejoin(стенд)
    assert стенд.нажатий == 2


def test_не_вернувшееся_устройство_роняет_teardown_с_уликой() -> None:
    стенд = Возврат(НЕ_ПОДКЛЮЧИЛСЯ, НЕ_ПОДКЛЮЧИЛСЯ)
    with pytest.raises(AssertionError, match='не вернулось в сеть стенда за 2 сеанса') as отказ:
        Stand.rejoin(стенд, tries=2)
    assert 'устройство не подключилось к сети (2 попыток' in str(отказ.value)
    assert стенд.нажатий == 2


def test_совпавший_конфиг_без_подключения_не_считается_сетью() -> None:
    with pytest.raises(AssertionError, match='подключиться к сети оно не смогло'):
        Stand.ensure_network(Подъём(joined=False))


def test_подключённое_устройство_не_перенастраивают() -> None:
    assert Stand.ensure_network(Подъём(joined=True)) is False
