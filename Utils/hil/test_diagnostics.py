"""
Плашки главной страницы портала - блок B ручного плана.

Прошивка сама ищет две ошибки настройки и показывает их на главной: вес
импульса, похожий на завышенный вдесятеро, и вход, который не насчитал ни
одного импульса, пока сосед считал. Оба признака приезжают в `/api/main_status`
кодами 23 и 24, поэтому браузер не нужен - хватает AT-платы.

Расход накапливается до входа в портал: `check_setup` считает по снимку
импульсов, снятому при загрузке ЕСП (`core/diagnostics.cpp`), а импульсы,
поданные при открытом портале, в этот снимок уже не попадут.

Плашка «внештатно перезагрузился» (код 22) здесь не проверяется: в режиме
настройки она показывается всегда - известное расхождение, docs/known-gaps.md.
"""

from __future__ import annotations

import json
from typing import Any

import pytest

from . import portal as portal_mod

pytestmark = [pytest.mark.stand, pytest.mark.portal, pytest.mark.slow]

NAMUR = 0

# Пара весов обязательна: плашка появляется, только когда один вес ровно
# вдесятеро тяжелее другого (`diagnostics.cpp`, factor_too_big). Виновным
# считается тяжёлый канал.
FACTOR_HEAVY = 100
FACTOR_LIGHT = 10

# 20 импульсов - это и NEIGHBOUR_MIN_IMPULSES для молчащего входа, и 200
# литров при лёгком весе, то есть COMPARE_MIN_LITERS для сравнения расходов
IMPULSES = 20

FACTOR_TOO_BIG = '23'
INPUT_SILENT = '24'


def problems(board: Any) -> set[tuple[str, Any]]:
    """Плашки главной страницы: пара «код, номер входа»."""
    answer = board.get('/api/main_status', portal_mod.HOST)
    assert answer.status == 200, answer.status
    return {(str(item['error']), item.get('input'))
            for item in json.loads(answer.text)}


def fresh_readings(stand: Any, factor0: int, factor1: int) -> None:
    """
    Веса и свежие показания на обоих входах.

    Ввод показаний сдвигает точку отсчёта импульсов (`applyInputParameter`),
    то есть обнуляет расход, по которому прошивка ищет ошибки настройки. Без
    этого тест считал бы вместе с расходом соседних тестов.
    """
    stand.setup(channel=0, ctype=NAMUR, factor=factor0, value='10.000')
    stand.setup(channel=1, ctype=NAMUR, factor=factor1, value='20.000')


@pytest.fixture(autouse=True)
def atboard(cfg: Any) -> None:
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')


def test_B3_heavy_factor_is_suspicious(cfg: Any, stand: Any) -> None:
    """
    Вес импульса завышен вдесятеро: тяжёлый канал насчитал в разы больше.

    Здесь же проверяется, что плашка гаснет сама, стоит заново ввести
    показания: расход считается от этого момента, и исправленная настройка не
    требует ни перезагрузки, ни отдельной кнопки.
    """
    fresh_readings(stand, factor0=FACTOR_HEAVY, factor1=FACTOR_LIGHT)

    stand.dut.pulse(channel=0, count=IMPULSES)      # 2000 литров
    stand.dut.pulse(channel=1, count=IMPULSES)      # 200 литров

    with portal_mod.session(cfg, stand) as board:
        found = problems(board)
        assert (FACTOR_TOO_BIG, 0) in found, found
        assert (FACTOR_TOO_BIG, 1) not in found, (
            'подозревать надо тяжёлый канал, а не оба')

        # B5: показания введены заново - расход обнулён, плашке неоткуда взяться
        answer = board.post('/api/save?input=0&ch=11.000', portal_mod.HOST)
        assert answer.status == 200, answer.status
        assert not json.loads(answer.text).get('errors'), answer.text

        assert (FACTOR_TOO_BIG, 0) not in problems(board)


def test_B4_silent_input(cfg: Any, stand: Any) -> None:
    """
    Вход настроен, сосед считает, а этот молчит: так выглядит перепутанный тип
    входа и оборванный провод.
    """
    fresh_readings(stand, factor0=FACTOR_LIGHT, factor1=FACTOR_LIGHT)

    stand.dut.pulse(channel=1, count=IMPULSES)      # на входе 0 - ничего

    with portal_mod.session(cfg, stand) as board:
        found = problems(board)
        assert (INPUT_SILENT, 0) in found, found
        assert (INPUT_SILENT, 1) not in found, 'считающий вход молчащим не назовёшь'
