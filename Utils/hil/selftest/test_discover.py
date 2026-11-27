"""
Поиск плат стенда - без сети.

29 сентября роутер раздал новые адреса, METF уехала с записанного, и прогон
полчаса падал по таймауту: «METF 192.168.100.21 не отозвалась за 30 с».
Адрес в файле - последнее известное место платы, а не приговор.
"""

from __future__ import annotations

from typing import Any

import pytest
import requests

from .. import discover


class Ответ:
    """То немногое от requests.Response, что смотрит опознание."""

    def __init__(self, text: str, ok: bool = True) -> None:
        self.text = text
        self.ok = ok


@pytest.fixture
def тишина(monkeypatch: pytest.MonkeyPatch) -> None:
    """Ни имён, ни соседей: каждый ход поиска должен включаться явно."""
    monkeypatch.setattr(discover, 'by_name', lambda name: '')
    monkeypatch.setattr(discover, 'sweep', lambda near, answers: '')


def отвечают(monkeypatch: pytest.MonkeyPatch, *hosts: str) -> list[str]:
    """Плата отзывается только с перечисленных адресов; кого спрашивали - в списке."""
    спрошены: list[str] = []

    def probe(host: str) -> bool:
        спрошены.append(host)
        return host in hosts

    monkeypatch.setattr(discover, 'BOARDS', {'metf': ('metf.local', probe)})
    return спрошены


def test_записанный_адрес_отвечает_и_поиска_не_будет(monkeypatch: pytest.MonkeyPatch,
                                                     тишина: None) -> None:
    """Обычный день: лишнего обхода сети быть не должно."""
    спрошены = отвечают(monkeypatch, '192.168.100.21')
    искали: list[str] = []
    monkeypatch.setattr(discover, 'sweep',
                        lambda near, answers: искали.append(near) or '')

    assert discover.find('metf', '192.168.100.21', '192.168.100.11') == '192.168.100.21'
    assert спрошены == ['192.168.100.21']
    assert искали == []


def test_молчащий_адрес_ищут_по_имени(monkeypatch: pytest.MonkeyPatch,
                                      тишина: None) -> None:
    отвечают(monkeypatch, '192.168.100.3')
    monkeypatch.setattr(discover, 'by_name',
                        lambda name: '192.168.100.3' if name == 'metf.local' else '')

    assert discover.find('metf', '192.168.100.21', '192.168.100.11') == '192.168.100.3'


def test_чужое_имя_не_принимают_на_веру(monkeypatch: pytest.MonkeyPatch,
                                        тишина: None) -> None:
    """Имя в сети мог занять кто угодно: адрес за ним обязан отозваться платой."""
    отвечают(monkeypatch, '192.168.100.3')
    monkeypatch.setattr(discover, 'by_name', lambda name: '192.168.100.77')
    monkeypatch.setattr(discover, 'sweep', lambda near, answers: '192.168.100.3')

    assert discover.find('metf', '', '192.168.100.11') == '192.168.100.3'


def test_без_имени_остаётся_обход_сети(monkeypatch: pytest.MonkeyPatch,
                                       тишина: None) -> None:
    """Прошивка старее пятнадцатого протокола имени не объявляет."""
    отвечают(monkeypatch, '192.168.100.3')
    monkeypatch.setattr(discover, 'sweep', lambda near, answers: '192.168.100.3')

    assert discover.find('metf', '192.168.100.21', '192.168.100.11') == '192.168.100.3'


def test_никого_не_нашли_значит_пусто(monkeypatch: pytest.MonkeyPatch,
                                      тишина: None) -> None:
    """Пустая строка, а не выдумка: отказ объяснит тот, кому плата нужна."""
    отвечают(monkeypatch)

    assert discover.find('metf', '192.168.100.21', '192.168.100.11') == ''


def test_обходят_свою_сеть_без_себя() -> None:
    соседи = list(discover.neighbours('192.168.100.11'))

    assert len(соседи) == 253
    assert '192.168.100.11' not in соседи
    assert соседи[0] == '192.168.100.1' and соседи[-1] == '192.168.100.254'


def test_обход_останавливается_на_первой_плате(monkeypatch: pytest.MonkeyPatch) -> None:
    def probe(host: str) -> bool:
        return host.endswith('.3')

    assert discover.sweep('192.168.100.11', probe) == '192.168.100.3'


def test_METF_узнают_по_версии_протокола(monkeypatch: pytest.MonkeyPatch) -> None:
    """`/version` отдаёт голое число - его не спутать с чужим веб-сервером."""
    monkeypatch.setattr(requests, 'get', lambda url, timeout: Ответ('15\n'))

    assert discover.metf_answers('192.168.100.3') is True


def test_чужой_веб_сервер_за_METF_не_принимают(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(requests, 'get', lambda url, timeout: Ответ('<html>роутер</html>'))

    assert discover.metf_answers('192.168.100.4') is False


def test_молчащий_адрес_не_плата(monkeypatch: pytest.MonkeyPatch) -> None:
    def отказ(url: str, timeout: float) -> Any:
        raise requests.ConnectionError('нет маршрута')

    monkeypatch.setattr(requests, 'get', отказ)

    assert discover.metf_answers('192.168.100.21') is False
