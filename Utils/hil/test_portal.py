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
from pathlib import Path
from typing import Any, Iterator

import pytest

from . import portal as portal_mod
from .atboard import AtBoard

pytestmark = [pytest.mark.stand, pytest.mark.portal]

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'ESP8266' / 'data'


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
    want = portal_mod.tree_version(ROOT)
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
