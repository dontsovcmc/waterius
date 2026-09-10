"""
Сторожевой таймер режима настройки - K3 ручного плана.

Портал живёт десять минут с последнего действия пользователя
(`core/portal_watchdog.h`). Проверяется не закрытие, а сам счёт: `/api/portal_time`
отдаёт, сколько секунд осталось, и по нему видно и продление, и его отсутствие.
Ждать десять минут ради того же утверждения незачем - что срок однажды выйдет,
проверяют хостовые тесты правила (`ESP8266/test/test_portal_watchdog`).

Продлевает окно действие пользователя: переход по странице и сохранение формы.
Опрос состояния входа - нет, и это не мелочь: страницы определения счётчика и
ввода показаний опрашивают вход каждые две секунды сами, и кормись таймер
опросом, забытая вкладка держала бы Wi-Fi включённым, пока не сядут батарейки.
На этом же исключении держится решение обойтись одним таймером, поэтому «опрос
не продлевает» проверяется наравне с «действие продлевает».

Сам `/api/portal_time` таймер тоже не кормит - иначе наблюдение стало бы
действием, и тест проверял бы себя.
"""

from __future__ import annotations

import json
import time
from typing import Any, Iterator

import pytest
from loguru import logger

from . import portal as portal_mod
from .atboard import AtBoard

pytestmark = [pytest.mark.stand, pytest.mark.portal]

# core/portal_watchdog.h
WATCHDOG_S = 600

# Пауза между замерами. Остаток отдаётся целыми секундами, так что хватает
# нескольких: разница обязана быть заметно больше единицы
STEP_S = 5


@pytest.fixture
def portal(cfg: Any, stand: Any) -> Iterator[AtBoard]:
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    with portal_mod.session(cfg, stand) as device:
        yield device


def left(board: AtBoard) -> int:
    """Сколько секунд осталось порталу. Запрос таймер не кормит."""
    answer = board.get('/api/portal_time', portal_mod.HOST)
    if answer.status == 404:
        pytest.skip('в прошивке нет /api/portal_time: прошейте свежую сборку')
    assert answer.status == 200, answer.status
    return int(json.loads(answer.text)['idle_left'])


def test_K3_action_extends_the_window(portal: AtBoard) -> None:
    """
    Действие пользователя продлевает окно, опрос состояния входа - нет.

    Оба утверждения читаются из одного числа: осталось стало больше - окно
    продлилось, осталось стало меньше - нет.
    """
    started = left(portal)
    assert started > WATCHDOG_S - 120, (
        f'портал только открыт, а осталось всего {started} с')

    time.sleep(STEP_S)
    portal.get('/api/status/1', portal_mod.HOST)
    polled = left(portal)
    logger.info(f'после опроса состояния: {started} -> {polled}')

    assert polled < started, (
        f'опрос состояния продлил окно: было {started} с, стало {polled} с')

    portal.get('/index.html', portal_mod.HOST)
    fed = left(portal)
    logger.info(f'после перехода по странице: {polled} -> {fed}')

    assert fed > polled, (
        f'страница не продлила окно: было {polled} с, стало {fed} с')
    assert fed > started, (
        f'окно продлилось не до полного: {fed} с при сроке {WATCHDOG_S} с')
