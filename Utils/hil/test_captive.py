"""
Captive portal без телефона - K1a-K1d, автоматическая часть K1.

Телефон, подключившись к точке, сам ничего не открывает: он спрашивает свой
проверочный адрес (`connectivitycheck.gstatic.com/generate_204`,
`captive.apple.com/hotspot-detect.html`...) и ждёт эталонного ответа - 204 или
«Success». Получил редирект - значит, сеть с порталом, и окно всплывает.
Цепочка держится на трёх звеньях прошивки (`active_point.cpp`), и все три
AT-плата проверяет так же, как телефон:

1. DHCP точки выдаёт клиенту в качестве DNS саму точку, а `DNSServer`
   отвечает её адресом на любое имя. Без первого клиент спрашивает свой DNS
   по умолчанию, и до портала его запрос не доходит.
2. Проверочный адрес каждой ОС отвечает редиректом на портал.
3. Корень `/` отдаёт ту посадочную страницу, которая положена по состоянию.

Чего здесь нет и не будет: самого окна и того, как мини-браузер айфона без
JavaScript рисует страницы, - это `04_not-tested.md`, §7. Что посадочные
страницы обходятся без скриптов, проверяет `ESP8266/scripts/test_strings.js`.
"""

from __future__ import annotations

from collections.abc import Iterator
from typing import Any

import pytest

from . import portal as portal_mod
from .atboard import AtBoard, AtError
from .constants import REPO_ROOT

pytestmark = [pytest.mark.stand, pytest.mark.portal]

DATA = REPO_ROOT / 'ESP8266' / 'data'


@pytest.fixture(scope='module')
def board(cfg: Any, stand: Any) -> Iterator[AtBoard]:
    """Портал на весь модуль: тесты его состояния не меняют."""
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    with portal_mod.session(cfg, stand) as device:
        yield device


def test_K1a_dns_answers_any_name(board: AtBoard) -> None:
    """Любое имя в сети портала ведёт на портал."""
    wrong = {}
    for name in portal_mod.FOREIGN_NAMES:
        try:
            address = board.resolve(name)
        except AtError as err:
            address = str(err)
        if address != portal_mod.HOST:
            wrong[name] = address
    assert not wrong, (
        f'DNS портала отпустил имена мимо {portal_mod.HOST}: {wrong}. '
        f'Плата спрашивает {board.dns()!r} - если это не {portal_mod.HOST}, '
        'DHCP точки не выдаёт себя как DNS, и телефон окна не покажет')


def test_K1b_os_probes_redirect_to_portal(board: AtBoard) -> None:
    """
    Проверочные адреса ОС отвечают редиректом, а не эталоном. Запрос идёт по
    имени, как у телефона: соединение открывается через DNS портала, и в
    заголовке `Host` то же имя.
    """
    wrong = []
    for name, path, status, location in portal_mod.PROBES:
        try:
            answer = board.get(path, name)
        except AtError as err:
            wrong.append(f'{name}{path}: {err}')
            continue
        got = answer.headers.get('location')
        if answer.status != status or got != location:
            wrong.append(f'{name}{path}: {answer.status} Location={got!r}, '
                         f'ожидался {status} Location={location!r}')
    assert not wrong, 'проверочные адреса ответили не так:\n' + '\n'.join(wrong)


def test_K1c_landing_page_follows_state(board: AtBoard) -> None:
    """
    После редиректа браузер окна идёт на `/`, и там его ждёт страница по
    состоянию: «Приступить» для ненастроенного устройства, обычный вход для
    настроенного. Признак настройки читается из `/api/main_status`, а не из
    лога прошивки: тест не должен верить тому, что проверяет.

    Портал только что поднят, подключений из него не было, поэтому страницы
    ошибки и успешного подключения здесь быть не может - ошибку проверяет W2 (K1d).
    """
    fresh = portal_mod.needs_setup(board)
    assert fresh is not None, (
        '/api/main_status сообщает об ошибке подключения на свежем портале: '
        'по нему не понять, настроен ли вход')
    want = portal_mod.LANDING_START if fresh else portal_mod.LANDING_CONFIGURED

    got = portal_mod.landing_page(board, DATA, host=portal_mod.PROBES[0][0])

    assert got == want, (
        f'вход {"не " if fresh else ""}настроен, а / отдал {got or "не посадочную страницу"} '
        f'вместо {want}')
