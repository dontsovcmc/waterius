"""
Портал настройки на живом устройстве (блок P ручного плана).

Что здесь проверяется и чего здесь нет. Браузера у стенда по-прежнему нет:
разметка не рисуется, скрипты не выполняются, кнопки не нажимаются. Зато
проверяется то, что до сих пор не проверялось ничем, - что образ LittleFS в
устройстве действительно тот, который лежит в репозитории, и что каждая
страница отдаётся целиком и без следов шаблонизатора.

Все запросы делаются один раз на модуль: одна страница едет через AT-плату
несколько секунд, и повторять обход в каждом тесте бессмысленно.
"""

from __future__ import annotations

import json
from collections.abc import Iterator
from typing import Any

import pytest

from . import portal as portal_mod
from .atboard import AtBoard
from .constants import REPO_ROOT
from .test_wizard import networks_list

pytestmark = [pytest.mark.stand, pytest.mark.portal]

DATA = REPO_ROOT / 'ESP8266' / 'data'


@pytest.fixture(scope='module')
def board(cfg: Any, stand: Any) -> Iterator[AtBoard]:
    """Портал на весь модуль: одна страница едет через AT-плату секунды."""
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    with portal_mod.session(cfg, stand) as device:
        yield device


@pytest.fixture(scope='module')
def pages(board: AtBoard) -> dict[str, portal_mod.Result]:
    return {r.url: r for r in portal_mod.check_pages(board)}


@pytest.fixture(scope='module')
def assets(board: AtBoard) -> dict[str, portal_mod.Result]:
    return {r.url: r for r in portal_mod.check_assets(board, DATA)}


@pytest.fixture(scope='module')
def api(board: AtBoard) -> dict[str, portal_mod.Result]:
    return {r.url: r for r in portal_mod.check_api(board)}


def test_P1_pages_open(pages: dict[str, portal_mod.Result], stand: Any) -> None:
    """Каждая страница портала отдаётся целиком."""
    version = stand.esp_version
    broken = []
    for url, result in pages.items():
        since = portal_mod.PAGE_SINCE.get(url)
        if since and (version is None or version < since):
            continue
        if result.status != 200 or not result.size:
            broken.append(f'{url}: {result.status} {result.size} байт {result.error}')
    assert not broken, 'страницы не открылись:\n' + '\n'.join(broken)


def test_P2_no_placeholders(pages: dict[str, portal_mod.Result]) -> None:
    """
    Шаблонизатор подставил всё. Незнакомый прошивке %NAME% остаётся в тексте
    как есть - пользователь увидит его на странице.
    """
    left = {url: r.placeholders for url, r in pages.items() if r.placeholders}
    assert not left, f'остались плейсхолдеры: {left}'


def test_P3_assets_match_image(assets: dict[str, portal_mod.Result], stand: Any) -> None:
    """
    Статика в устройстве совпадает с образом байт в байт.

    Прошивка и файловая система шьются разными командами, и залитый по ошибке
    старый образ ничем больше не виден: страницы открываются, версия в логе
    правильная, а стили и картинки - от предыдущего выпуска.
    """
    want = portal_mod.tree_version(REPO_ROOT)
    if want is None or stand.esp_version != want:
        pytest.skip(f'на устройстве {stand.version_str}, в дереве '
                    f'{".".join(map(str, want)) if want else "?"} - сверять нечего')
    diff = [f'{url}: {r.size} байт против {r.expected} в образе'
            for url, r in assets.items() if not r.same]
    assert not diff, 'файлы разошлись с образом:\n' + '\n'.join(diff)


def test_P4_api_answers(api: dict[str, portal_mod.Result]) -> None:
    """
    API портала отвечает разбираемым JSON, а статус входа не жалуется на связь
    с attiny: `error` там появляется, когда i2c не отвечает (active_point_api.cpp).
    """
    for url, result in api.items():
        assert result.status == 200, f'{url}: {result.status}'
        try:
            body = json.loads(result.body.decode('utf-8'))
        except ValueError as err:
            pytest.fail(f'{url}: не JSON ({err}): {result.body[:120]!r}')
        if url.startswith('/api/status/'):
            assert 'error' not in body, f'{url}: {body}'
            assert {'state', 'factor', 'impulses'} <= set(body), f'{url}: {body}'


def test_P5_saved_password_is_masked(board: AtBoard, cfg: Any) -> None:
    """
    Сохранённый пароль Wi-Fi страница отдаёт звёздочками, а не текстом (#108, #307).

    Точка настройки открыта, пароля у неё нет: разметку видит любой, кто
    подключился. `%password%` прошивка подставляет как `********`, если пароль
    задан (`active_point.cpp`, processor), а те же звёздочки при сохранении
    означают «не трогали» (`core/input.h`, is_all_asterisks) - это проверяет C6.
    """
    assert cfg.dut_password, 'пароль сети устройства в stand.ini'
    for url in ('/wifi_settings.html', '/setup_send.html'):
        answer = board.get(url, portal_mod.HOST)
        assert answer.status == 200, f'{url}: {answer.status}'
        assert cfg.dut_password not in answer.text, f'{url}: пароль Wi-Fi виден в разметке'

    page = board.get('/wifi_settings.html', portal_mod.HOST).text
    assert 'value="********"' in page, (
        'поле пароля пустое, хотя пароль сохранён: форма уйдёт без него, и '
        'прошивка примет пустой пароль за новый')


def test_P6_log_journal(board: AtBoard, stand: Any) -> None:
    """
    Журнал портала - живой лог, а не заглушка из образа: в нём есть строка о
    самом этом запросе. Отдаётся текстом, не вложением (W2b - зачем).
    """
    want = portal_mod.tree_version(REPO_ROOT)
    if want is None or stand.esp_version != want:
        pytest.skip(f'на устройстве {stand.version_str}: журнала может не быть')
    answer = board.get('/api/log', portal_mod.HOST)
    assert answer.status == 200, f'/api/log: {answer.status}'
    assert 'content-disposition' not in answer.headers, answer.headers
    assert 'GET /api/log' in answer.text, answer.text[-400:]
    assert 'ssid_len:' not in answer.text, 'список сетей попал на экран'


def test_P6b_log_download_has_networks(board: AtBoard, stand: Any) -> None:
    """
    Кнопка «Скачать журнал» отдаёт журнал и за ним весь /ssid.txt: файл из флеша
    идёт тем же потоком, что и кольцо в памяти, и не должен обрываться.
    """
    want = portal_mod.tree_version(REPO_ROOT)
    if want is None or stand.esp_version != want:
        pytest.skip(f'на устройстве {stand.version_str}: журнала может не быть')
    networks_list(board)  # /ssid.txt пишется при отдаче списка после скана
    ssid_file = board.get('/ssid.txt', portal_mod.HOST)
    assert ssid_file.status == 200, f'/ssid.txt: {ssid_file.status}'
    answer = board.get('/api/log?networks=1', portal_mod.HOST)
    assert answer.status == 200, f'/api/log?networks=1: {answer.status}'
    assert 'content-disposition' not in answer.headers, answer.headers
    assert 'GET /api/log' in answer.text, answer.text[:400]
    assert answer.text.endswith(ssid_file.text), answer.text[-400:]
