"""
Wi-Fi в рабочем режиме: роутер меняет канал и имя - блок W вне портала.

Быстрый коннект (#222, #372): после подключения прошивка запоминает канал и
BSSID и первую попытку делает по ним (`wifi_helpers.cpp`, wifi_begin). Неудача
обнуляет канал, и вторая попытка идёт полным сканом. Дефект здесь выглядит
как устройство, навсегда потерявшее сеть после смены канала на роутере, -
увидеть его можно только роутером, который канал меняет.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .logwatch import Session  # а сбор тестов должен работать без них
    from .stand import Stand

pytestmark = [pytest.mark.stand, pytest.mark.slow]

ATTEMPT = 'WIFI: Attempt #'


def other_channel(cfg: Any, current: int) -> int:
    """
    Запасной канал для смены канала - из настроек стенда, а не из правила.

    Раньше номер выбирался формулой «11, если сейчас не больше 6». Это был
    долг: на чужом стенде канал мог оказаться глухим - сильный сосед, и
    устройство не увидит сеть вовсе, а тест проверит удачу. Теперь оба канала
    стоят в stand.ini, выбраны по скану эфира и перебиваются аргументом
    --ap-channel-other.
    """
    other = int(cfg.ap_channel_other)
    assert other != current, (
        f'запасной канал {other} совпал с рабочим: смене канала некуда идти. '
        f'Поправьте ap_channel_other в stand.ini или --ap-channel-other')
    return other


@pytest.mark.requires(esp='2.0.47')
def test_W4_router_changed_channel(stand: Stand) -> None:
    """
    Роутер сменил канал: быстрый коннект промахивается, полный скан находит
    сеть, и следующий сеанс снова быстрый - уже на новом канале (#222, #372).
    """
    assert stand.last_payload is not None
    old = int(stand.last_payload['channel'])
    new = other_channel(stand.cfg, old)

    with stand.router.channel(new):
        stand.reset_observers()
        stand.dut.press_button()
        moved = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert f'WIFI: begin channel: {old}' in moved.text, moved.text
        assert moved.wifi_connected, f'полный скан не нашёл сеть на канале {new}\n{moved.text}'
        assert moved.payload is not None and int(moved.payload['channel']) == new

        stand.reset_observers()
        stand.dut.press_button()
        fast = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert f'WIFI: begin channel: {new}' in fast.text, fast.text
        assert fast.text.count(ATTEMPT) == 1, (
            f'на запомненном канале хватает одной попытки\n{fast.text}')


def teach(stand: Stand, ssid: str, password: str) -> Session:
    """Сообщить устройству сеть ответом приёмника, пока оно в прежней."""
    stand.reset_observers()
    stand.receiver.reply_settings({'ssid': ssid, 'password': password})
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    # Имя с пробелом `Saved:` регулярным выражением не разобрать - ищем строкой
    assert f'Saved: ssid={ssid}' in session.text, f'имя сети не сохранено\n{session.text}'
    assert f'Saved: password={password}' in session.text, (
        f'пароль сети не сохранён\n{session.text}')
    return session


# Имена и пароли, на которых прошивка уже ломалась. Пределы полей - core/types.h:
# WIFI_SSID_LEN 32 и WIFI_PWD_LEN 64, оба вместе с завершающим нулём
EDGE_NETWORKS = [
    pytest.param('Waterius Test', 'stand-pass-1', id='space'),              # #280
    pytest.param("word's word2", 'stand-pass-1', id='apostrophe'),          # #113
    pytest.param('hil-digits', '1234567890123456', id='digits_password'),   # #50, #114
    pytest.param('hil-long-pass', 'p' * 63, id='password_63'),             # предел WPA2
    pytest.param('W' * 31, 'stand-pass-1', id='ssid_31'),                 # PR #265
    pytest.param('W' * 32, 'stand-pass-1', id='ssid_32', marks=pytest.mark.xfail(
        strict=True,
        reason='WIFI_SSID_LEN 32 включает завершающий ноль: имя в 32 байта, '
               'законное по 802.11, parse_text отвергает по длине')),
]


@pytest.mark.requires(esp='2.0.47')
@pytest.mark.parametrize('ssid, password', EDGE_NETWORKS)
def test_W3_unusual_network_names(stand: Stand, ssid: str, password: str) -> None:
    """
    Сеть с пробелом, апострофом, паролем из цифр, предельной длины
    (#50, #113, #114, #280, PR #265).

    Устройство узнаёт новую сеть ответом приёмника, пока живёт в прежней,
    потом точка стенда переименовывается. Прежние имя и пароль возвращаются
    тем же путём, пока устройство ещё в новой сети; не вышло - порталом.
    """
    from .router import RouterError

    home_ssid, home_password = stand.ap_ssid, stand.cfg.ap_password
    teach(stand, ssid, password)

    back = False
    try:
        with stand.router.ssid_verbatim(ssid, password):
            stand.reset_observers()
            stand.dut.press_button()
            session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
            assert session.wifi_connected, f'в сеть {ssid!r} не подключился\n{session.text}'
            assert session.payload is not None, 'в сети, а посылки нет'

            teach(stand, home_ssid, home_password)
            back = True
    except RouterError as err:
        pytest.skip(f'точка стенда такое имя не держит: {err}')
    finally:
        if not back:
            stand.ensure_network(force=True)
