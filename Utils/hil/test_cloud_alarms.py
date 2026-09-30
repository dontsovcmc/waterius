"""
Тревоги через облако: устройство -> облако -> команда снятия -> устройство.

Облако здесь - стенда (`stand.cloud`): второй приёмник, которому устройство шлёт
вместо cloud.waterius.ru. Работу настоящего облака стенд не проверяет - только
то, за что отвечает прошивка: тревога ушла, ответ 200 прочитан, команда из
ответа применена.

Отличие от `test_alarms.py` в пути команды: там маска снятия едет от своего
сервера, здесь - из ответа облака. Прошивка складывает настройки из ответов
обоих получателей (`https_helpers.cpp`, post_data), и пустой ответ своего
сервера не должен стирать то, что прислало облако.

Блок Y. Запуск: ``pytest --stand Utils/hil/test_cloud_alarms.py``
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .constants import (
    ALARM_WAIT_S,
    LEAKAGE,
    PLANNED_PERIOD_MIN,
    PLANNED_WAIT_S,
    RESET_WET0,
    RESET_WET1,
)
from .logwatch import ALARM_MODE, SEND_OK, TRANSMIT_MODE
from .test_alarms import raise_both_channels

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand  # а сбор тестов должен работать без них

# Тревог до attiny 41 не существует, а статусы получателей поимённо печатает
# строка `Alarm confirm`, которой нет на ЕСП младше 2.0.47
pytestmark = [pytest.mark.stand, pytest.mark.requires(attiny=41, esp='2.0.47')]


@pytest.mark.needs(ctype1=LEAKAGE)
def test_Y1_wet_alarm_reaches_cloud(stand: Stand, quiet: None) -> None:
    """Сработавший датчик протечки доезжает до облака, и облако отвечает 200."""
    stand.reset_observers()

    try:
        stand.dut.wet(channel=1, closed=True)
        session = stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
    finally:
        stand.dut.wet(channel=1, closed=False)

    session.assert_confirm(waterius=SEND_OK)
    # Последний, а не единственный: оборванную в эфире попытку прошивка повторяет
    assert session.cloud_codes[-1:] == [200], (
        f'ответы облака: {session.cloud_codes}\n{session.text}')

    payload = stand.cloud.wait_payload(timeout=0)
    assert payload is not None, f'облако стенда не получило посылку\n{session.text}'
    assert payload['alarm_wet1'] is True, payload


@pytest.mark.slow
@pytest.mark.needs(ctype1=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
def test_Y3_clear_from_cloud_reaches_device(stand: Stand, quiet: None) -> None:
    """
    Маска снятия из ответа облака доезжает до attiny и гасит тревогу.

    Ждём плановый сеанс: кнопка снимает тревоги attiny сама, и тест был бы
    зелёным при любой прошивке.
    """
    stand.reset_observers()
    try:
        stand.dut.wet(channel=1, closed=True)
        stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE).assert_alarm(wet1=1)
    finally:
        # Пока вход замкнут, attiny поднимала бы тревогу заново
        stand.dut.wet(channel=1, closed=False)

    stand.reset_observers()
    stand.cloud.reply_settings({'arst': RESET_WET1})
    applied = stand.wait_session(timeout=PLANNED_WAIT_S, mode=TRANSMIT_MODE)
    assert applied.applied.get('arst') == str(RESET_WET1), (
        f'маска из ответа облака не применена: {applied.applied}\n{applied.text}')
    assert applied.alarm_config['reset'] == RESET_WET1, (
        f'маска не уехала в attiny: {applied.alarm_config}')

    # В этом сеансе посылка собрана из снимка, снятого до снятия тревоги
    stand.reset_observers()
    stand.wait_session(timeout=PLANNED_WAIT_S).assert_alarm(wet1=0)


@pytest.mark.slow
@pytest.mark.needs(ctype0=LEAKAGE, ctype1=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
def test_Y4_two_alarms_clear_by_one_mask(stand: Stand, quiet: None) -> None:
    """Сложенная маска из ответа облака снимает обе тревоги разом."""
    raise_both_channels(stand)

    mask = RESET_WET0 | RESET_WET1
    stand.reset_observers()
    stand.cloud.reply_settings({'arst': mask})
    applied = stand.wait_session(timeout=PLANNED_WAIT_S, mode=TRANSMIT_MODE)
    assert applied.alarm_config['reset'] == mask, (
        f'маска не уехала в attiny: {applied.alarm_config}\n{applied.text}')

    stand.reset_observers()
    stand.wait_session(timeout=PLANNED_WAIT_S).assert_alarm(wet0=0, wet1=0)
