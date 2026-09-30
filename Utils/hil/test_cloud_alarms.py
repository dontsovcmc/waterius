"""
Тревога доезжает до сервера и сервер её принимает - блок Y.

Сервер здесь - свой, то есть приёмник стенда: проверяется то, за что отвечает
прошивка, - внеплановая посылка ушла, в ней стоит тревога, ответ 200 прочитан.
Что с тревогой делает бэкенд, проверяют его тесты. От cloud.waterius.ru тест не
зависит и идёт с `--nocloud` тоже.

Снятие тревоги маской из ответа сервера - `test_E17` в `test_alarms.py`.

Запуск: ``pytest --stand Utils/hil/test_cloud_alarms.py``
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .constants import ALARM_WAIT_S, LEAKAGE
from .logwatch import ALARM_MODE, SEND_OK

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand  # а сбор тестов должен работать без них

# Тревог до attiny 41 не существует, а статусы получателей поимённо печатает
# строка `Alarm confirm`, которой нет на ЕСП младше 2.0.47
pytestmark = [pytest.mark.stand, pytest.mark.requires(attiny=41, esp='2.0.47')]


@pytest.mark.needs(ctype1=LEAKAGE)
def test_Y1_wet_alarm_reaches_server(stand: Stand, quiet: None) -> None:
    """Сработавший датчик протечки доезжает до своего сервера, ответ - 200."""
    stand.reset_observers()

    try:
        stand.dut.wet(channel=1, closed=True)
        session = stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
    finally:
        stand.dut.wet(channel=1, closed=False)

    session.assert_alarm(wet1=1)
    session.assert_confirm(http=SEND_OK)
    # Последний, а не единственный: оборванную в эфире попытку прошивка повторяет
    assert session.own_codes[-1:] == [200], (
        f'ответы своего сервера: {session.own_codes}\n{session.text}')
