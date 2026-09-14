"""
Автоопределение веса импульса холодной воды - A10 ручного плана.

«Авто» выбирает вес по тому, сколько импульсов вход насчитал с начала сеанса
настройки: до трёх включительно - 10 л/имп, от четырёх - 1 л/имп
(`core/readings.cpp`, get_auto_factor). Проверяются обе стороны порога, один
импульс и четыре, и обе - до веса в посылке.

Каждый случай - на своём заводском сбросе. Функция ветвится по сохранённому
весу, а не по присланному: «Авто» работает, пока вес не задан, и заданный вес
уже не перетирает. Незаданным он бывает только после сброса, а сброс оставляет
устройство в портале - там определение и проходит, как у человека в мастере.

Импульсы портал считает от снимка при загрузке ЕСП, а запись типа входа в
attiny счётчик не трогает: `set_counter_types` снимает только тревоги, и лишь
при смене типа. Граница порога не сдвигается.
"""

from __future__ import annotations

import time
from typing import Any

import pytest

from . import portal as portal_mod
from .constants import (AUTO_FACTOR_FEW, AUTO_FACTOR_MANY, AUTO_IMPULSE_FACTOR,
                        AUTO_LIMIT, COLD, NAMUR)

pytestmark = [pytest.mark.stand, pytest.mark.portal, pytest.mark.reset]


def status(board: Any) -> dict[str, Any]:
    """Состояние входа - то, что показывает страница определения счётчика."""
    body = portal_mod.get_json(board, f'/api/status/{COLD}')
    assert 'error' not in body, f'нет связи с attiny: {body}'
    return body


def wait_impulses(board: Any, want: int, timeout: float = 10.0) -> dict[str, Any]:
    """Дождаться, пока вход насчитает want импульсов с начала сеанса настройки."""
    deadline = time.time() + timeout
    while True:
        body = status(board)
        if body['impulses'] >= want or time.time() >= deadline:
            return body
        time.sleep(1)


@pytest.mark.parametrize('pulses, factor',
                         [(1, AUTO_FACTOR_FEW), (AUTO_LIMIT + 1, AUTO_FACTOR_MANY)],
                         ids=['1_impulse', '4_impulses'])
def test_A10_auto_factor_by_pulse_count(fresh_device: Any, stand: Any,
                                        pulses: int, factor: int) -> None:
    """
    Вес «Авто» - по числу импульсов при настройке (#69, #78, #339).

    Там же #346: пока вес не задан, выбор типа входа ведёт на определение
    счётчика. Обратное - повторная настройка сразу к показаниям - проверяет W1.
    """
    board = fresh_device.board

    answer = portal_mod.post_json(board, '/api/save_input_type', input=COLD, ctype=NAMUR)
    assert not answer.get('errors'), answer
    assert answer.get('redirect') == f'/input/{COLD}/detect.html', (
        f'первая настройка минует определение счётчика: {answer}')

    start = status(board)
    assert start['impulses'] == 0, (
        f"до первого импульса вход уже насчитал {start['impulses']}: "
        'граница порога сдвинута, проверка потеряла бы смысл')

    stand.dut.pulse(channel=COLD, count=pulses)
    now = wait_impulses(board, pulses)
    assert now['impulses'] == pulses, f'импульсы не досчитались: {now}'
    assert now['factor'] == factor, (
        f"{pulses} имп. - это {factor} л/имп, подсказка {now['factor']}")

    answer = portal_mod.post_json(board, '/api/save', input=COLD,
                                  factor=AUTO_IMPULSE_FACTOR)
    assert not answer.get('errors'), answer

    session = fresh_device.leave()
    assert session.payload['f1'] == factor, (
        f"сохранён вес {session.payload['f1']}, а «Авто» при {pulses} имп. "
        f'обязан дать {factor}')
