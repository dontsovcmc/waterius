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


class FakeClient:
    """Плата, падающая заданное число раз подряд, а затем отвечающая."""

    def __init__(self, failures: int, error: Exception | None = None) -> None:
        self.failures = failures
        self.error = error or requests.ConnectionError('host is down')
        self.calls = 0

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
