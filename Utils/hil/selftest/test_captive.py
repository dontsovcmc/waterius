"""
Разбор ответов для K1a-K1d (captive portal). Железо не нужно.

На стенде ошибка разбора выглядела бы как ошибка прошивки: «DNS отпустил
имя», «/ отдал не ту страницу». Поэтому разбор проверяется отдельно.
"""

from __future__ import annotations

import json
from typing import Any

import pytest

from .. import atboard
from .. import portal as portal_mod
from ..atboard import Response
from ..constants import REPO_ROOT

DATA = REPO_ROOT / 'ESP8266' / 'data'


class Replies:
    """AT-плата, у которой ответ на команду задан заранее."""

    def __init__(self, answer: str) -> None:
        self.answer = answer
        self.sent: list[str] = []

    def cmd(self, line: str, timeout: float = 10.0) -> str:
        self.sent.append(line)
        return self.answer


@pytest.mark.parametrize('answer', [
    'AT+CIPDOMAIN="captive.apple.com"\r\n+CIPDOMAIN:"192.168.4.1"\r\n\r\nOK\r\n',
    'AT+CIPDOMAIN="captive.apple.com"\r\n+CIPDOMAIN:192.168.4.1\r\n\r\nOK\r\n',
])
def test_resolve_разбирает_адрес(answer: str) -> None:
    fake = Replies(answer)
    assert atboard.AtBoard.resolve(fake, 'captive.apple.com') == '192.168.4.1'
    assert fake.sent == ['AT+CIPDOMAIN="captive.apple.com"']


def test_resolve_без_адреса_это_ошибка() -> None:
    """Неразрешённое имя - `ERROR` без адреса; тишиной это считать нельзя."""
    with pytest.raises(atboard.AtError):
        atboard.AtBoard.resolve(Replies('AT+CIPDOMAIN="x"\r\nERROR\r\n'), 'x')


def test_страница_узнаётся_по_образу() -> None:
    body = (DATA / portal_mod.LANDING_START).read_bytes()
    assert portal_mod.landing_matches(body, portal_mod.LANDING_START, DATA)
    assert not portal_mod.landing_matches(body, portal_mod.LANDING_CONFIGURED, DATA)


def test_подстановка_не_мешает_узнать_страницу_ошибки() -> None:
    """У страницы ошибки процессор: вместо %wifi_connect_status% приезжает код."""
    text = (DATA / portal_mod.LANDING_ERROR).read_text(encoding='utf-8')
    assert '%wifi_connect_status%' in text
    body = text.replace('%wifi_connect_status%', '9').encode()
    assert portal_mod.landing_matches(body, portal_mod.LANDING_ERROR, DATA)


class MainStatus:
    """Портал, у которого /api/main_status отдаёт заданные плашки."""

    def __init__(self, codes: list[str]) -> None:
        self.body = json.dumps([{'error': c} for c in codes]).encode()

    def get(self, path: str, host: str, timeout: float = 30.0) -> Response:
        assert path == '/api/main_status'
        return Response(status=200, body=self.body)


@pytest.mark.parametrize('codes, want', [
    (['3'], True),            # «Ватериус ещё не настроен»
    (['2'], True),            # «подключился, теперь настроим счётчики»
    ([], False),
    (['24'], False),          # чужая плашка о настройке не говорит
    (['1'], None),            # ошибка подключения: про вход прошивка молчит
])
def test_признак_настройки(codes: list[str], want: Any) -> None:
    assert portal_mod.needs_setup(MainStatus(codes)) is want
