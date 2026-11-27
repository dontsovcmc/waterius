"""
Ключевые события устройства попадают в лог прогона по ходу дела.

Прежде лог прогона знал только приговоры стенда - «сеанс не доиграл за 125 с»,
«приёмник пуст», - а чем устройство было занято эти две минуты, в нём не стояло
ни строки: лог Ватериуса печатался лишь в отказ, и то хвостом. Разбор прогона
29 сентября упёрся ровно в это: ЕСП загрузилась без attiny, сказала об этом
одной строкой, и строка утонула - тест сорок раз обвинил связь.

Метки нарезаются из того же потока, что и сеансы (`LogWatcher.poll`), поэтому
проверяются они так же: образцы режутся кольцом METF.
"""

from __future__ import annotations

import time

import pytest

from .. import logwatch
from ..logwatch import MANUAL_TRANSMIT_MODE, LogWatcher
from .test_logwatch import FakeApi, fw, through_ring

СЕАНС = [
    fw('Startup mode: 3'),
    fw('attiny firmware ver: 41'),
    fw('WIFI: Connected.'),
    fw('WIFI: SSID: waterius_stand Channel: 6 BSSID: 3c:61:05:0a:0b:0c mode: 11N'),
    fw('MQTT: Connected.'),
    fw('MQTT: pub waterius/ch0 size=5 retain=1'),
    fw('MQTT: Publish data finished: 12 topics, 340 ms'),
    fw('HTTP: Response code: 200'),
    fw('WATR: Data sent. Time 812 ms'),
    fw('Going to sleep'),
]

БЕЗ_ATTINY = [
    fw('ESP firmware ver: 2.0.51'),
    fw('end error:2', level='ERROR'),
    fw('Attiny not found.', level='ERROR'),
    fw('Blynk: code=5'),
    fw('Going to sleep'),
]


@pytest.fixture
def сказано(monkeypatch: pytest.MonkeyPatch) -> list[str]:
    """Всё, что стенд написал в лог прогона за тест."""
    said: list[str] = []
    monkeypatch.setattr(logwatch.logger, 'info', lambda text: said.append(text))
    monkeypatch.setattr(logwatch.logger, 'warning', lambda text: said.append(text))
    return said


def прогнать(lines: list[str]) -> LogWatcher:
    """Отдать стенду лог и дать ему дочитать последнюю строку."""
    watcher = LogWatcher(FakeApi(through_ring(lines)))
    watcher.poll()
    watcher.poll()          # плата молчит: последняя строка дописана
    return watcher


def test_метки_рассказывают_сеанс_по_порядку(сказано: list[str]) -> None:
    прогнать(СЕАНС)

    события = [text for text in сказано if text.startswith('Ватериус:')]
    assert len(события) == 7, события
    assert 'режим 3' in события[0] and 'кнопка' in события[0]
    assert 'waterius_stand' in события[1] and 'канал 6' in события[1]
    assert 'брокер' in события[2]
    assert '12' in события[3]
    assert '200' in события[4]
    assert 'waterius.ru' in события[5] and '812' in события[5]
    assert 'сон' in события[6]


def test_разрезанная_строка_метится_целиком(сказано: list[str]) -> None:
    """
    Кольцо METF режет строку по 60 символов, а канал сети стоит в самом конце.
    Метить кусок - значит потерять ровно то, за чем метка и заводилась.
    """
    прогнать(СЕАНС)

    assert any('канал 6' in text for text in сказано), сказано


def test_метка_не_повторяется(сказано: list[str]) -> None:
    """Опрос идёт дважды в секунду: помеченная строка второй метки не рождает."""
    watcher = прогнать(СЕАНС)
    было = len(сказано)
    watcher.poll()
    watcher.poll()

    assert len(сказано) == было, сказано[было:]


def test_ошибка_прошивки_попадает_в_лог(сказано: list[str]) -> None:
    """
    Про `end error:2` стенд не знает ничего, кроме того, что это ошибка, - и
    этого хватает: в логе прогона она встаёт рядом с тем, что стенд делал.
    """
    прогнать(БЕЗ_ATTINY)

    assert any('end error:2' in text for text in сказано), сказано


def test_потерянная_attiny_названа_словами(сказано: list[str]) -> None:
    прогнать(БЕЗ_ATTINY)

    метка = [text for text in сказано if 'attiny' in text]
    assert метка, сказано
    assert 'i2c' in метка[0], метка


def test_ожидание_сеанса_не_сидит_потолок_без_attiny() -> None:
    """
    ЕСП без attiny сеанса не начнёт никогда: `Startup mode:` печатается только
    после её ответа. Ждать тут нечего - ни секунды из потолка.
    """
    watcher = LogWatcher(FakeApi(through_ring(БЕЗ_ATTINY)))

    started = time.monotonic()
    assert watcher.wait_session(timeout=600, mode=MANUAL_TRANSMIT_MODE,
                                poll_interval=0.01) is None
    assert time.monotonic() - started < 5, 'ждали потолок, хотя attiny не отвечает'
    assert watcher.attiny_lost


def test_целый_сеанс_на_attiny_не_жалуется() -> None:
    watcher = прогнать(СЕАНС)
    assert not watcher.attiny_lost
