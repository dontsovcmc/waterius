"""
Фикстуры селф-тестов.

Селф-тесты нарочно гоняют стенд по путям отказа, и его предупреждения в логе
прогона выглядят как настоящие поломки: «эфир: скан не вышел», «AT-плата: радио
осталось включённым: порт отвалился», «соединение с 192.168.4.1 не открылось».
Две трети всех WARNING полного прогона - отсюда. Поэтому записи селф-тестов
помечаются, а разбирающий лог видит пометку, а не ищет беду, которой нет.
"""

from __future__ import annotations

from collections.abc import Iterator

import pytest
from loguru import logger


@pytest.fixture(autouse=True)
def _поддельно() -> Iterator[None]:
    with logger.contextualize(поддельно=True):
        yield
