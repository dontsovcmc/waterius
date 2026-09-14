"""
Заводской сброс - блок R.

`POST /api/reset` из портала стирает EEPROM и конфигурацию Wi-Fi самой ЕСП, а
восстанавливает ровно одно - уникальный токен устройства (`config.cpp`,
factory_reset). Это и проверяется: токен пережил сброс, всё остальное вернулось
к умолчаниям `init_config`.

Сам сброс и возврат стенда - в `reset.py`: тем же сбросом пользуются A10
(`test_auto_factor.py`) и E10 (`test_alarms.py`), которым нужен незаданный вес
импульса.
"""

from __future__ import annotations

from typing import Any, Iterator

import pytest

from .constants import AS_COLD_CHANNEL, AUTO_IMPULSE_FACTOR, DEFAULT_WAKEUP_PERIOD_MIN
from .reset import FreshDevice

pytestmark = [pytest.mark.stand, pytest.mark.reset, pytest.mark.slow,
              pytest.mark.requires(attiny=41)]


@pytest.fixture(scope='module')
def after_reset(cfg: Any, stand: Any) -> Iterator[dict]:
    """
    Первый сеанс после заводского сброса, один на модуль: R1 и R2 читают одну
    посылку.

    Сеть и адрес приёмника возвращаются сразу: без них не будет ни посылки, ни
    следующего теста. Всё остальное - вес, период, типы входов, брокер -
    намеренно остаётся сброшенным, это и есть предмет проверки.
    """
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    device = FreshDevice.reset(cfg, stand)
    try:
        session = device.leave()
        yield {'before': device.before, 'session': session, 'payload': session.payload}
    finally:
        device.close()


def test_R1_token_survives_reset(after_reset: dict) -> None:
    """
    Токен - единственное, что переживает сброс.

    Им устройство опознаётся в облаке, и потерять его значит потерять привязку
    к аккаунту: показания поедут в никуда, а владелец узнает об этом не сразу.
    """
    was = after_reset['before']['key']
    now = after_reset['payload']['key']
    assert now == was, f'токен изменился: был {was}, стал {now}'


def test_R2_settings_return_to_defaults(after_reset: dict) -> None:
    """
    Всё остальное вернулось к умолчаниям.

    Список широкий намеренно. Сброс возвращает структуру настроек целиком, и
    проверка по двум-трём полям зеленела бы на любом частичном сбросе: пороги
    тревог, показания, серийные номера и маска квитанции лежат в тех же
    настройках и сбрасываться обязаны так же.

    Типы входов сюда же, хотя живут они в EEPROM attiny: сбросить их ЕСП может
    только командой, и без неё «заводское состояние» осталось бы с датчиком
    протечки на входе.
    """
    payload = after_reset['payload']
    expected = {
        'f0': AS_COLD_CHANNEL,          # вес обоих входов - снова спецзначения
        'f1': AUTO_IMPULSE_FACTOR,
        'period_min': DEFAULT_WAKEUP_PERIOD_MIN,
        'ctype0': 0, 'ctype1': 0,       # типы входов - в EEPROM attiny
        'ch0_start': 0, 'ch1_start': 0,  # показания
        'serial0': '', 'serial1': '',
        'company': '', 'place': '',
        'av0': 0, 'av1': 0,             # пороги тревог
        'ar0': 0, 'ar1': 0,
        'ah0': 0, 'ah1': 0,
        'as0': 0, 'as1': 0,
        'vac': False, 'sc': False,
        'ackw': False, 'ackh': False,   # маска квитанции - CONFIRM_ANY
        'ackm': False,
        'voltage_cal': 100,
        'mqtt': False,                  # брокер выключен, адрес стенда забыт
        'ha': False,
    }
    wrong = {name: payload.get(name) for name, want in expected.items()
             if payload.get(name) != want}
    assert not wrong, f'после сброса осталось не умолчание: {wrong}'
