"""
Что стенд считает доказательством, что портал после сброса поднялся.

28 сентября доказательство было одно - строка в логе устройства, - и стенд сам
же её стёр: `clear()` чистит кольцо METF, а перезапуск начинается через доли
секунды после ответа на `/api/reset`. Точка при этом стояла в эфире, но спросить
о ней было некого, и две минуты ожидания кончились отказом, после которого
устройство осталось без сети на двадцать минут.

Отсюда два свидетеля и короткий срок: METF видит перезапуск и подъём точки,
AT-плата видит ту же точку в эфире, а ждать вообще нечего, если METF не отдала
после сброса ни строки.
"""

from __future__ import annotations

from typing import Any

import pytest

from .. import portal as portal_mod
from .. import reset as reset_mod
from ..atboard import AtError, Network, Response


class ПоддельныйЛог:
    """Окно лога, наполняющееся порциями: одна порция - один `poll()`."""

    def __init__(self, порции: list[list[str]]) -> None:
        self.порции = list(порции)
        self.lines: list[str] = []

    def poll(self) -> None:
        if self.порции:
            self.lines.extend(self.порции.pop(0))

    def tail(self, count: int = 30) -> str:
        return '\n'.join(self.lines[-count:])


class ПоддельныйСтенд:
    def __init__(self, порции: list[list[str]]) -> None:
        self.log = ПоддельныйЛог(порции)
        self.last_payload = {'esp_id': '6827706', 'version_esp': '2.0.51'}
        self.ap_ssid = 'waterius_stand'


class ПоддельнаяПлата:
    """AT-плата, у которой спрашивают только скан."""

    def __init__(self, сети: list[Network]) -> None:
        self.сети = сети
        self.сканов = 0

    def scan(self, ssid: str | None = None, timeout: float = 30.0) -> list[Network]:
        self.сканов += 1
        return [net for net in self.сети if ssid is None or net.ssid == ssid]


ТОЧКА = 'waterius-6827706-2.0.51'


def поднялась(канал: int) -> list[str]:
    return [f'00:00:601-041/00%  INFO  : AP started on channel={канал} , '
            f'ssid={ТОЧКА}']


def эфир(канал: int) -> list[Network]:
    return [Network(ssid=ТОЧКА, rssi=-42, bssid='aa:bb:cc:dd:ee:ff', channel=канал)]


@pytest.fixture(autouse=True)
def _без_пауз(monkeypatch: pytest.MonkeyPatch) -> None:
    """Часы идут от сна: сроки здесь проверяются, а ждать их незачем."""
    часы = {'t': 0.0}

    def спать(сек: float) -> None:
        часы['t'] += max(сек, 0.1)

    monkeypatch.setattr(reset_mod.time, 'sleep', спать)
    monkeypatch.setattr(reset_mod.time, 'monotonic', lambda: часы['t'])


def test_оба_свидетеля_согласны(monkeypatch: pytest.MonkeyPatch) -> None:
    stand = ПоддельныйСтенд([поднялась(1)])
    board = ПоддельнаяПлата(эфир(1))

    assert reset_mod.wait_portal(None, stand, board) == ТОЧКА
    assert board.сканов == 1, 'эфир не спросили вовсе'


def test_молчащая_METF_не_стоит_ожидания(monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Ни строки после сброса - ждать точку незачем: либо устройство не
    перезагрузилось, либо стенд ослеп. Обе беды видны сразу, а не через две
    минуты.
    """
    stand = ПоддельныйСтенд([])
    board = ПоддельнаяПлата(эфир(1))
    monkeypatch.setattr(portal_mod, 'rescue_stray_portal',
                        lambda *a, **kw: (True, '. устройство возвращено'))

    with pytest.raises(AssertionError, match='не отдала ни строки'):
        reset_mod.wait_portal(None, stand, board)
    assert board.сканов == 0, 'скан эфира при слепом стенде бессмыслен'


def test_точка_на_чужом_канале_это_улика(monkeypatch: pytest.MonkeyPatch) -> None:
    """
    После сброса канал точки - единица: заводское `wifi_channel` плюс
    `ap_channel`. Другой канал значит, что настройки сброс пережили.
    """
    stand = ПоддельныйСтенд([поднялась(11)])
    board = ПоддельнаяПлата(эфир(11))

    with pytest.raises(AssertionError, match='канале 1'):
        reset_mod.wait_portal(None, stand, board)


def test_свидетели_разошлись(monkeypatch: pytest.MonkeyPatch) -> None:
    """METF видит точку, эфир пуст: молчать об этом нельзя."""
    stand = ПоддельныйСтенд([поднялась(1)])
    board = ПоддельнаяПлата([])

    with pytest.raises(AssertionError, match='не.*находит её в эфире'):
        reset_mod.wait_portal(None, stand, board)


def test_имя_точки_считается_по_посылке() -> None:
    """Скан по имени вместо скана всего эфира: имя известно из посылки."""
    stand = ПоддельныйСтенд([])
    assert portal_mod.device_ap_name(stand) == ТОЧКА


class ПлатаФормы:
    """Плата, роняющая первый POST и отвечающая на второй."""

    def __init__(self, осечек: int) -> None:
        self.осечек = осечек
        self.posts: list[bytes] = []

    def post(self, path: str, host: str, body: bytes = b'', **kw: Any) -> Response:
        self.posts.append(body)
        if self.осечек:
            self.осечек -= 1
            raise AtError("это не ответ HTTP: b''")
        return Response(status=200, headers={}, body=b'{}')


def test_потерянный_ответ_формы_это_повтор() -> None:
    """
    Пустой ответ не значит «форма не дошла»: 28 сентября в логе устройства были
    видны её параметры. Сохранения портала идемпотентны, повтор их не портит.
    """
    board = ПлатаФормы(осечек=1)
    assert portal_mod._save(board, '/api/save', portal_mod.HOST, http_on='1') == {}
    assert len(board.posts) == 2, 'форма не повторена'
    assert board.posts[0] == board.posts[1], 'повторили не то же самое'


def test_вторая_осечка_формы_уже_отказ() -> None:
    board = ПлатаФормы(осечек=2)
    with pytest.raises(AtError):
        portal_mod._save(board, '/api/save', portal_mod.HOST, http_on='1')


class ПлатаСпасения(ПоддельнаяПлата):
    """Плата, через которую устройство возвращают в сеть стенда."""

    def __init__(self, сети: list[Network]) -> None:
        super().__init__(сети)
        self.вошла: str | None = None
        self.пути: list[str] = []
        self.закрыта = False

    def join(self, ssid: str, password: str = '', timeout: float = 30.0) -> str:
        self.вошла = ssid
        return '192.168.4.2'

    def post(self, path: str, host: str, body: bytes = b'', **kw: Any) -> Response:
        self.пути.append(path)
        return Response(status=200, headers={}, body=b'{}')

    def get(self, path: str, host: str, **kw: Any) -> Response:
        self.пути.append(path)
        return Response(status=200, headers={}, body=b'')

    def close(self) -> None:
        self.закрыта = True


class ПоддельныйCfg:
    atboard_port = '/dev/fake'
    ap_password = '12345678'
    http_url = 'http://192.168.100.18:8010/data'


def test_незамеченный_портал_возвращают_в_сеть(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Погасить портал мало. 28 сентября `/api/turnoff` вывел из настроек
    устройство со стёртой сетью, и сорок семь тестов подряд падали на пустом
    приёмнике. Сперва сеть и приёмник, и только потом выход.
    """
    board = ПлатаСпасения(эфир(1))
    monkeypatch.setattr(portal_mod, 'AtBoard', lambda *a, **kw: board)

    вернули, улика = portal_mod.rescue_stray_portal(
        ПоддельныйCfg(), ПоддельныйСтенд([]))

    assert вернули and 'возвращено в сеть стенда' in улика
    assert board.вошла == ТОЧКА
    assert board.пути == ['/api/save_connect', '/api/save', '/api/turnoff'], (
        'до выхода из настроек обязаны уехать сеть и приёмник')
    assert board.закрыта, 'своя плата осталась открытой'


def test_пустой_эфир_не_повод_лезть_в_точку(
        monkeypatch: pytest.MonkeyPatch) -> None:
    board = ПлатаСпасения([])
    monkeypatch.setattr(portal_mod, 'AtBoard', lambda *a, **kw: board)

    вернули, улика = portal_mod.rescue_stray_portal(
        ПоддельныйCfg(), ПоддельныйСтенд([]))

    assert not вернули and 'в эфире нет' in улика
    assert board.вошла is None and board.пути == []
