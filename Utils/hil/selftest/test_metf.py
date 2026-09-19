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
    """Ответ платы: стенду от него нужен только `raise_for_status`."""

    def raise_for_status(self) -> None:
        return None


class FakeSession:
    """`requests.Session` на минималках: помнит запросы и падает по заказу."""

    def __init__(self, failures: int = 0, error: Exception | None = None) -> None:
        self.failures = failures
        self.error = error or requests.ConnectionError('host is down')
        self.posts: list[tuple[str, dict, float]] = []

    def post(self, url: str, data: dict, timeout: float) -> FakeAnswer:
        self.posts.append((url, data, timeout))
        if len(self.posts) <= self.failures:
            raise self.error
        return FakeAnswer()


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
