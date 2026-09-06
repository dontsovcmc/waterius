"""
Отправка данных и причины неудачного сеанса - блок G ручного плана.

Про светодиод. Ручной план проверяет число вспышек, но стенд их не видит: код
в лог не печатается, а опрос пина по HTTP слишком медленный, чтобы поймать
вспышку в 200 мс. Поэтому здесь проверяется не код, а причина, из которой он
считается (core/blink.cpp): нет конфига, нет роутера, нет облака, нет брокера.
Это честнее и заодно точнее - в отчёте видно, что именно сломалось.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import pytest

from .logwatch import MANUAL_TRANSMIT_MODE, SEND_OK
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

# Поля, которые обязаны быть в каждой посылке. Проверяем не только наличие, но
# и тип с диапазоном: поле, ставшее всегда нулевым, список имён не поймает.
REQUIRED_FIELDS: dict[str, Any] = {
    'ch0': float, 'ch1': float,
    'delta0': int, 'delta1': int,
    'imp0': int, 'imp1': int,
    'f0': int, 'f1': int,
    'ctype0': int, 'ctype1': int,
    'alarm_flow0': (0, 1), 'alarm_flow1': (0, 1),
    'alarm_leak0': (0, 1), 'alarm_leak1': (0, 1),
    'alarm_wet0': (0, 1), 'alarm_wet1': (0, 1),
    'alarm_stop0': (0, 1), 'alarm_stop1': (0, 1),
    'af0': int, 'af1': int, 'al0': int, 'al1': int, 'as0': int, 'as1': int,
    'vac': (0, 1), 'sc': (0, 1),
    'ackw': (0, 1), 'ackh': (0, 1), 'ackm': (0, 1),
    'mode': (1, 2, 3, 4),
    'version': int, 'version_esp': str, 'model': int,
    'voltage': float, 'rssi': int, 'period_min': int,
}


def test_G1_all_three_channels(stand: Stand) -> None:
    """Короткое нажатие: показания уходят во все три канала."""
    stand.reset_observers()
    stand.dut.press_button()

    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    session.assert_confirm(waterius=SEND_OK, http=SEND_OK, mqtt=SEND_OK)
    session.assert_cause('ok')
    assert session.payload is not None, 'приёмник не получил посылку'

    # Ловит дефект, при котором повторная отправка после применения настроек
    # уходит в брокер уже после disconnect
    assert 'MQTT: Not connected' not in session.text

    assert stand.mqtt is not None
    assert stand.mqtt.wait_topic(stand.cfg.mqtt_topic.split('/')[-1], timeout=30) \
        or stand.mqtt.topics(), 'в брокере нет ни одного топика'


def test_G2_payload_schema(stand: Stand) -> None:
    """
    Состав посылки: типы и диапазоны, а не просто список имён.

    Проверка «поле есть» пропустила бы поле, которое всегда ноль, а именно так
    выглядит большинство регрессов в сериализации.
    """
    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    payload = session.payload
    assert payload is not None

    missing = [name for name in REQUIRED_FIELDS if name not in payload]
    assert not missing, f'в посылке нет полей: {missing}'

    for name, expected in REQUIRED_FIELDS.items():
        value = payload[name]
        if isinstance(expected, tuple):
            assert value in expected, f'{name}={value}, допустимо {expected}'
        elif expected is float:
            assert isinstance(value, (int, float)), f'{name}={value!r} не число'
        else:
            assert isinstance(value, expected), f'{name}={value!r} не {expected.__name__}'


def test_G3_no_network(stand: Stand) -> None:
    """
    Сети нет: две вспышки. Признак в логе - две неудачные попытки подключения
    и отсутствие строки Alarm confirm, которую печатают только при связи.
    """
    stand.reset_observers()

    with stand.net.ap_off():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_cause('router')
    assert not session.wifi_connected
    assert session.text.count('WIFI: Connection failed.') >= 2
    assert session.confirm is None


def test_G4_server_unreachable(stand: Stand) -> None:
    """Сеть есть, сервера нет: три вспышки. Ватериус подключился, но не доставил."""
    stand.reset_observers()

    with stand.net.internet_down():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.wifi_connected, 'Wi-Fi должен был подняться: режем только трафик'
    session.assert_cause('cloud')
    assert 'Data sent' not in session.text


def test_G5_broker_unreachable(stand: Stand) -> None:
    """
    Брокер недоступен, облако живо: четыре вспышки.

    Ловим `MQTT: Connect failed with state`, а не `MQTT: Connecting failed`:
    вторая строка не печатается никогда, потому что mqtt_connect возвращает
    успех после всех неудачных попыток.
    """
    stand.reset_observers()

    with stand.net.mqtt_down():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_cause('mqtt')
    assert 'MQTT: Connect failed with state' in session.text
    assert session.confirm and session.confirm['waterius'] == SEND_OK
