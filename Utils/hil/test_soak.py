"""
Многочасовой прогон: устройство живёт само и не теряет ни сеансов, ни счёта.

Короткие тесты проверяют одно пробуждение. Дефекты этого класса проявлялись
через сутки и позже: attiny переставала будить ЕСП (#170, #183, #278), шина
i2c сбоила (#5, #7, #75), устройство переставало подключаться до нажатия
кнопки (#204). Видны они только на длинной дистанции, поэтому прогон отдельный:
`pytest Utils/hil --stand --soak`, длительность - `--soak-minutes`.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest

from .logwatch import TRANSMIT_MODE

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand  # а сбор тестов должен работать без них

pytestmark = [pytest.mark.stand, pytest.mark.slow, pytest.mark.soak]

SOAK_PERIOD_MIN = 5

# Строки прошивки о сбое обмена с attiny (master_i2c.cpp). В прогоне без смены
# типа входа ни одной быть не должно
I2C_ERRORS = ('I2C transmitting fail', 'end error:', 'RequestFrom failed',
              'Attiny not found', 'Send CMD failed', 'CRC wrong', 'Data failed')

# Сеанса нет дольше полутора периодов - attiny его пропустила
MISSED_FACTOR = 1.5


def test_Z1_soak(stand: Stand, request: pytest.FixtureRequest) -> None:
    """Плановые сеансы без пропусков, без сбоев i2c, с растущим счётом."""
    minutes = request.config.getoption('--soak-minutes')
    stand.setup(period_min=SOAK_PERIOD_MIN)
    stand.reset_observers()

    started = time.time()
    deadline = started + minutes * 60
    impulses: tuple[int, int] | None = None
    count = 0
    while time.time() < deadline:
        session = stand.wait_session(
            timeout=SOAK_PERIOD_MIN * 60 * MISSED_FACTOR + 120, mode=TRANSMIT_MODE)
        count += 1
        where = f'сеанс {count}, {(time.time() - started) / 60:.0f} мин прогона'

        failures = [line for line in session.lines
                    if any(marker in line for marker in I2C_ERRORS)]
        assert not failures, f'{where}: сбой i2c\n' + '\n'.join(failures)
        assert session.complete and session.wifi_connected, f'{where}\n{session.text}'
        assert session.payload is not None, f'{where}: посылки нет'

        now = (session.payload['imp0'], session.payload['imp1'])
        if impulses is not None:
            assert now[0] >= impulses[0] and now[1] >= impulses[1], (
                f'{where}: счёт пошёл назад {impulses} -> {now}')
        impulses = now

        # Иначе за двенадцать часов лог и очередь приёмника копятся в памяти
        stand.reset_observers()
