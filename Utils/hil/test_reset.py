"""
Заводской сброс - блок R.

`POST /api/reset` из портала стирает EEPROM и конфигурацию Wi-Fi самой ЕСП, а
восстанавливает ровно одно - уникальный токен устройства (`config.cpp`,
factory_reset). Это и проверяется: токен пережил сброс, всё остальное вернулось
к умолчаниям `init_config`, а показания attiny обнулились.

Сам сброс и возврат стенда - в `reset.py`: тем же сбросом пользуются A10
(`test_auto_factor.py`) и E10 (`test_alarms.py`), которым нужен незаданный вес
импульса.
"""

from __future__ import annotations

from collections.abc import Iterator
from typing import Any

import pytest

from . import portal as portal_mod
from .constants import (
    AS_COLD_CHANNEL,
    AUTO_IMPULSE_FACTOR,
    DEFAULT_WAKEUP_PERIOD_MIN,
    ESP_RESTARTED,
    RST_SOFT_RESTART,
    WATER_COLD,
    WATER_HOT,
)
from .logwatch import parse_reset_reason
from .reset import FreshDevice

pytestmark = [pytest.mark.stand, pytest.mark.reset,
              pytest.mark.requires(attiny=44)]


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
        # Загрузку и плашки снимаем в портале, до выхода из него: потом их не будет
        boot = '\n'.join(stand.log.lines)
        banners = {str(item.get('error'))
                   for item in portal_mod.get_json(device.board, '/api/main_status')}
        session = device.leave()
        # Судим по первой посылке: во второй уже то, что ответил приёмник
        yield {'before': device.before, 'session': session,
               'payload': session.payloads[0], 'tail': stand.log.tail(60),
               'boot': boot, 'banners': banners}
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

    Проверяется ровно то, что сбрасывает `factory_reset` (`config.cpp`):
    настройки ЕСП целиком - присваиванием `Settings()` и `init_config`, - плюс
    типы входов, которые живут в EEPROM attiny и стираются только командой
    `setCountersType`. Показания attiny проверяет R3.

    Вне списка три поля, которые к первой посылке успел задать стенд: сеть,
    `http_on` и `http_url`, - без них посылки не было бы вовсе. И
    `setup_finished`: счётчик растёт на выходе из портала, то есть уже после
    сброса.
    """
    payload = after_reset['payload']
    expected = {
        'f0': AS_COLD_CHANNEL,          # вес обоих входов - снова спецзначения
        'f1': AUTO_IMPULSE_FACTOR,
        'period_min': DEFAULT_WAKEUP_PERIOD_MIN,
        'cname0': WATER_HOT, 'cname1': WATER_COLD,
        'ctype0': 0, 'ctype1': 0,       # типы входов - в EEPROM attiny
        'ch0_start': 0, 'ch1_start': 0,  # показания
        'serial0': '', 'serial1': '',
        'company': '', 'place': '',
        'email': '',                    # переживает сброс только токен
        'av0': 0, 'av1': 0,             # пороги тревог
        'ar0': 0, 'ar1': 0,
        'ah0': 0, 'ah1': 0,
        'as0': 0, 'as1': 0,
        'vac': False, 'sc': False,
        'ackw': False, 'ackh': False,   # маска квитанции - CONFIRM_ANY
        'ackm': False,
        'voltage_cal': 100,
        'dhcp': True, 'mqtt_retain': True,
        'mqtt': False,                  # брокер выключен, адрес стенда забыт
        'ha': False,
    }
    wrong = {name: payload.get(name) for name, want in expected.items()
             if payload.get(name) != want}
    assert not wrong, (f'после сброса осталось не умолчание: {wrong}\n'
                       f'{after_reset["tail"]}')


@pytest.mark.requires(attiny=47, esp='2.0.53')
def test_R3_attiny_impulses_cleared(after_reset: dict) -> None:
    """
    Сброс обнуляет и показания attiny (#326).

    Они живут в её EEPROM кольцом, и при старте побеждает блок с наибольшими
    счётчиками: записать один нулевой блок мало, старые вернулись бы. Поэтому
    attiny стирает всё кольцо по команде 'R'. Нули в первой посылке после
    сброса показывают, что команда дошла и стирание завершилось: ЕСП после
    сброса перезагружалась, и attiny отдала показания заново.
    """
    was = (after_reset['before'].get('imp0'), after_reset['before'].get('imp1'))
    if not any(was):
        pytest.skip(f'до сброса импульсов не было ({was}): нули ничего не докажут')
    payload = after_reset['payload']
    now = (payload.get('imp0'), payload.get('imp1'))
    assert now == (0, 0), (f'импульсы после сброса {now}, до сброса {was}\n'
                           f'{after_reset["tail"]}')


@pytest.mark.requires(esp='2.0.53')
def test_R4_reset_is_not_a_crash(after_reset: dict) -> None:
    """
    После сброса портал не пугает плашкой «внештатно перезагрузился».

    Сброс перезагружает ЕСП при живом питании, и attiny честно отвечает
    «питание подано давно». Штатной перезагрузку прошивка узнаёт по своей же
    причине ресета: `factory_reset` зовёт `ESP.restart()`, и загрузка видит
    RST_SOFT_RESTART (`core/restart.cpp`). Причина сверяется отдельно - это
    слово SDK, а не наше.
    """
    reason = parse_reset_reason(after_reset['boot'])
    assert reason, f'в логе загрузки после сброса нет причины ресета\n{after_reset["boot"]}'
    code, restarted = reason
    assert code == RST_SOFT_RESTART, (
        f'после ESP.restart() причина ресета {code}, а не {RST_SOFT_RESTART}')
    assert not restarted, 'прошивка сочла сброс внештатной перезагрузкой'
    assert ESP_RESTARTED not in after_reset['banners'], (
        f'плашка о внештатной перезагрузке после сброса: {sorted(after_reset["banners"])}')
