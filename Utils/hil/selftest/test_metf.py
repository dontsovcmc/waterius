"""
Клиент METF с повторами. Железо не нужно: плату заменяет заглушка.

Проверяется ровно то, ради чего обёртка написана: короткая осечка связи - повод
повторить, долгое молчание - повод остановить прогон, а ошибка не про связь
должна лететь как есть, без повторов и без задержек.
"""

from __future__ import annotations

import pytest
import requests

from .. import metf


class Clock:
    """Часы под управлением теста: `sleep` двигает время, а не ждёт."""

    def __init__(self) -> None:
        self.now = 1000.0

    def time(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeAnswer:
    """Ответ платы: стенду нужны `raise_for_status` и код - `202` у `/pulse`."""

    def __init__(self, status_code: int = 202) -> None:
        self.status_code = status_code

    def raise_for_status(self) -> None:
        return None


class FakeSession:
    """`requests.Session` на минималках: помнит запросы и падает по заказу."""

    def __init__(self, failures: int = 0, error: Exception | None = None,
                 status_code: int = 202) -> None:
        self.failures = failures
        self.error = error or requests.ConnectionError('host is down')
        self.status_code = status_code
        self.posts: list[tuple[str, dict, float]] = []

    def post(self, url: str, data: dict, timeout: float) -> FakeAnswer:
        self.posts.append((url, data, timeout))
        if len(self.posts) <= self.failures:
            raise self.error
        return FakeAnswer(self.status_code)


class FakeClient:
    """Плата, падающая заданное число раз подряд, а затем отвечающая."""

    def __init__(self, failures: int, error: Exception | None = None,
                 session: FakeSession | None = None) -> None:
        self.failures = failures
        self.error = error or requests.ConnectionError('host is down')
        self.calls = 0
        self._root = 'http://192.0.2.1'
        self._sess = session or FakeSession()

    def ping(self) -> str:
        self.calls += 1
        if self.calls <= self.failures:
            raise self.error
        return 'pong'


@pytest.fixture
def clock(monkeypatch: pytest.MonkeyPatch) -> Clock:
    fake = Clock()
    monkeypatch.setattr(metf, 'time', fake)
    return fake


def board(monkeypatch: pytest.MonkeyPatch, client: FakeClient, **kw: object) -> metf.Metf:
    monkeypatch.setattr(metf, 'METFClient', lambda host: client)
    return metf.Metf('192.0.2.1', **kw)      # type: ignore[arg-type]


def test_короткая_осечка_переживается_повтором(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Плата отвалилась на секунду - тест не должен из-за этого падать."""
    client = FakeClient(failures=1)
    api = board(monkeypatch, client)
    assert api.ping() == 'pong'
    assert client.calls == 2, 'повтора не было'


def test_молчание_дольше_срока_останавливает_прогон(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата не отвечает дольше отведённого - дальше идти незачем: без METF стенд
    не может ничего, и остальные тесты дадут тот же traceback.
    """
    client = FakeClient(failures=99)
    api = board(monkeypatch, client, attempts=5, pause=1.0, dead_after=2.0)
    with pytest.raises(metf.MetfGone) as err:
        api.ping()
    assert '192.0.2.1' in str(err.value)


def test_ошибка_не_про_связь_не_повторяется(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Повторять осмысленно обрыв связи, а не ошибку вызова."""
    client = FakeClient(failures=1, error=ValueError('плохой аргумент'))
    api = board(monkeypatch, client)
    with pytest.raises(ValueError):
        api.ping()
    assert client.calls == 1, 'ошибку вызова повторять нельзя'


def test_импульс_повторяется_если_соединение_не_установилось(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Плата не приняла соединение - запроса она не видела, можно повторить."""
    session = FakeSession(failures=1)
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    api.pulse(pin=1, value=0, duration_ms=500)
    assert len(session.posts) == 2, 'повтора не было'
    assert session.posts[0][0] == 'http://192.0.2.1/pulse'


def test_импульс_не_повторяется_после_read_timeout(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Запрос ушёл, ответа нет: импульс мог быть выдан. Повтор нажал бы кнопку
    второй раз - за длинным нажатием это режим настройки вместо передачи.
    """
    session = FakeSession(failures=99, error=requests.ReadTimeout('no answer'))
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    with pytest.raises(requests.ReadTimeout):
        api.pulse(pin=1, value=0, duration_ms=4000)
    assert len(session.posts) == 1, 'импульс повторять нельзя'


def test_нажатие_кнопки_идёт_через_обёртку(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Иначе самое частое действие стенда остаётся единственным без повторов."""
    from ..dut import Dut

    session = FakeSession(failures=1)
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    dut = Dut(api, button_pin=1, ch0_pin=3, ch1_pin=2, reset_pin=0)
    dut._led_ok = False           # индикатора у заглушки нет, он тут не проверяется
    dut.press_button()
    assert len(session.posts) == 2, 'нажатие прошло мимо повторов'


def test_выдержку_импульса_ждёт_стенд(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата отвечает сразу, и конец импульса - забота клиента. Не подождать
    значит тронуть ещё занятую линию: следующий импульс сольётся с этим.
    """
    session = FakeSession()
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    started = clock.now
    api.pulse(pin=1, value=0, duration_ms=2000)
    assert clock.now - started == pytest.approx(2.0), (
        'стенд не выдержал паузу до конца импульса')


def test_старая_прошивка_платы_видна_сразу(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    До протокола 8 плата отвечала концом импульса, и ответ опаздывал на четверть
    секунды. Промолчать об этом - значит отдать прогон с поехавшими паузами и
    падениями там, где прошивка Ватериуса ни при чём.
    """
    session = FakeSession(status_code=200)
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    with pytest.raises(metf.MetfTooOld) as err:
        api.pulse(pin=1, value=0, duration_ms=500)
    assert 'протокол' in str(err.value)


class FakeBoard:
    """Плата, отвечающая заданным состоянием на `/version` и `/wifi`."""

    def __init__(self, version: int = 10, **wifi: object) -> None:
        self.version = version
        self.wifi = {'state': 'online', 'mode': 'sta', 'connected': True,
                     'ssid': 'dav', 'source': 'build', 'rssi': -62,
                     'ap_up': False, 'problem': 'none', 'hw_error': False}
        self.wifi.update(wifi)
        self.gets: list[str] = []

    def get(self, url: str, timeout: float) -> FakeRead:
        self.gets.append(url)
        if url.endswith('/version'):
            return FakeRead(text=str(self.version))
        if url.endswith('/wifi'):
            return FakeRead(payload=self.wifi)
        raise AssertionError(f'неожиданный запрос {url}')


class FakeRead:
    """Ответ на чтение: текст версии или JSON состояния."""

    def __init__(self, text: str = '', payload: dict | None = None) -> None:
        self.text = text
        self._payload = payload

    def raise_for_status(self) -> None:
        return None

    def json(self) -> dict:
        assert self._payload is not None
        return self._payload


@pytest.fixture
def warnings(monkeypatch: pytest.MonkeyPatch) -> list[str]:
    """Предупреждения стенда списком: проверяем не только отказы, но и слова."""
    said: list[str] = []
    monkeypatch.setattr(metf.logger, 'warning', said.append)
    monkeypatch.setattr(metf.logger, 'info', lambda text: None)
    return said


def inspected(monkeypatch: pytest.MonkeyPatch, plate: FakeBoard,
              stand_ssid: str = 'waterius_stand', stand_channel: int = 0) -> str:
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    return metf.check(api, '192.0.2.1', stand_ssid, stand_channel)


def test_досмотр_валит_прошивку_младше_восьмого_протокола(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Старую плату надо ловить на старте, а не по кривым паузам между импульсами:
    до восьмого протокола ответ приходил после конца импульса и опаздывал на
    четверть секунды.
    """
    with pytest.raises(metf.MetfTooOld, match='протокол'):
        inspected(monkeypatch, FakeBoard(version=7))


def test_досмотр_валит_плату_на_точке_стенда(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Управляющий канал не должен лежать на том, что тесты ломают: первое же
    выключение точки уносит вместе с устройством и саму METF. Отказ обязан
    называть лекарство - иначе плату придётся доставать пробросом порта.
    """
    with pytest.raises(metf.MetfOnStandAp) as err:
        inspected(monkeypatch, FakeBoard(ssid='waterius_stand'))
    assert 'action=forget' in str(err.value)


def test_досмотр_пропускает_плату_в_домашней_сети(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """Здоровая плата не должна ни падать, ни жаловаться."""
    summary = inspected(monkeypatch, FakeBoard())
    assert 'протокол 10' in summary and 'обрывы: none' in summary
    assert warnings == []


def test_слабый_сигнал_остаётся_предупреждением(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Прогон на -78 дБм разваливался на сетевых тестах, но отказывать здесь
    нельзя: запас сигнала - повод предупредить, а не повод не пустить к железу.
    """
    summary = inspected(monkeypatch, FakeBoard(rssi=-78))
    assert '-78' in summary
    assert any('-78' in line for line in warnings), warnings


def test_запомненная_сеть_названа_предупреждением(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Запомненная порталом сеть перекрывает зашитую при сборке и переживает
    перезагрузку: после прерванного прогона плата вернётся не туда, где её
    ищет stand.ini.
    """
    inspected(monkeypatch, FakeBoard(source='saved'))
    assert any('forget' in line for line in warnings), warnings


def test_поднятая_точка_платы_видна_с_причиной(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """Своя точка METF означает потерю сети; причину протокол 10 говорит словами."""
    inspected(monkeypatch, FakeBoard(ap_up=True, problem='dropped'))
    assert any('dropped' in line for line in warnings), warnings


def test_девятый_протокол_досматривается_без_причины_обрыва(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """Поле `problem` появилось в десятом: на девятом его нет, и врать о нём нельзя."""
    summary = inspected(monkeypatch, FakeBoard(version=9))
    assert 'обрывы' not in summary and 'протокол 9' in summary


def test_восьмой_протокол_не_спрашивают_о_сети(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """До девятого ручки `/wifi` нет вовсе - запрос к ней ответит 404."""
    plate = FakeBoard(version=8)
    summary = inspected(monkeypatch, plate)
    assert summary == 'протокол 8'
    assert not any(url.endswith('/wifi') for url in plate.gets), plate.gets


def test_перекрывающиеся_каналы_названы_причиной(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Точка стенда на 6, домашняя сеть на 4 - замер 23 сентября 2026. Так и было:
    сегменты гибли, тела посылок приходили обрезанными, чтение лога проваливалось
    на секунду. Стенд обязан называть это сам, иначе ищут дефект прошивки.
    """
    inspected(monkeypatch, FakeBoard(channel=4), stand_channel=6)
    assert any('перекрываются' in line for line in warnings), warnings


def test_разнесённые_каналы_молчат(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """1 и 6 не перекрываются - жаловаться не на что."""
    inspected(monkeypatch, FakeBoard(channel=1), stand_channel=6)
    assert warnings == []


def test_общий_канал_не_считается_бедой(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    На общем канале платы слышат друг друга и делят эфир по очереди. Это хуже
    разнесённых каналов по скорости, но не рвёт передачи, а ругаться на то, что
    само по себе не ломает прогон, - значит приучить не читать предупреждения.
    """
    inspected(monkeypatch, FakeBoard(channel=6), stand_channel=6)
    assert warnings == []


class SilentReader:
    """Плата, которая приняла `GET /read`, но ответа не прислала."""

    def __init__(self, error: Exception | None = None) -> None:
        self.error = error or requests.ReadTimeout('no answer')
        self.gets = 0

    def get(self, url: str, timeout: float) -> FakeRead:
        self.gets += 1
        raise self.error


def test_чтение_лога_не_повторяется_после_таймаута(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата осушает кольцо, отдавая ответ. Ответ не доехал - строки потеряны, и
    повтор вернёт уже следующие: в логе окажется дыра, о которой никто не знает.
    """
    plate = SilentReader()
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    with pytest.raises(requests.ReadTimeout):
        api.serial_read()
    assert plate.gets == 1, 'чтение лога повторять нельзя'
    assert api.log_holes == 1, 'дыра в логе не посчитана'


def test_отказ_соединения_при_чтении_дырой_не_считается(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Соединение не открылось - плата запроса не видела, кольцо цело."""
    plate = SilentReader(requests.ConnectionError('host is down'))
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    with pytest.raises(requests.ConnectionError):
        api.serial_read()
    assert plate.gets == 3, 'отказ соединения повторить можно и нужно'
    assert api.log_holes == 0, 'целый лог объявлен дырявым'
