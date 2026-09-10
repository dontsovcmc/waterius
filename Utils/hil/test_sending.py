"""
Отправка данных и неудачные сеансы - блок G ручного плана.

Вспышки светодиода здесь не проверяются вовсе: всё читается из лога. Он и
точнее - в отчёте видно, какой именно получатель не ответил, а не число
вспышек, общее для двух разных поломок.

Четыре несчастья, четыре разных отпечатка в логе:

* нет роутера - `WIFI: Connection failed.` и ни одной строки `Alarm confirm`:
  её печатают только после успешного подключения;
* нет облака waterius.ru - `Alarm confirm: ... waterius=3 http=1`;
* нет своего сервера - та же строка, но `waterius=1 http=3`;
* нет брокера - `MQTT: Connect failed with state` и `mqtt=3`;
* сервер отвечает не двумястами - `http=2` и `HTTP: Response code: 500`.

Различать облако и свой сервер по строке `HTTP: Send OK` нельзя: её печатают
оба, одним и тем же текстом (senders/send_data.cpp). Единственное место, где
они разведены поимённо, - `Alarm confirm`.

Строка `Blynk: code=N` говорит, какой код прошивка собралась моргать
(wleds.cpp). Проверяется именно решение: **число вспышек стенд не видит** и не
проверяет - у него нет счётчика фронтов на выводе светодиода. Наглядное
подтверждение, зачем нужны статусы получателей: у G4a и G4b код один и тот же,
то есть глазами эти две поломки неразличимы.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import pytest

from .logwatch import (BLYNK_CLOUD, BLYNK_CLOUD_ANSWER, BLYNK_MQTT,
                       BLYNK_ROUTER, MANUAL_TRANSMIT_MODE, SEND_BAD_ANSWER,
                       SEND_NO_CONNECTION, SEND_OK)
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

# Поля, которые обязаны быть в каждой посылке. Проверяем не только наличие, но
# и тип с диапазоном: поле, ставшее всегда нулевым, список имён не поймает.
# sender_http.h: столько раз прошивка повторяет отправку, пока не получит 200
HTTP_SEND_ATTEMPTS = 3

REQUIRED_FIELDS: dict[str, Any] = {
    'ch0': float, 'ch1': float,
    'delta0': int, 'delta1': int,
    'imp0': int, 'imp1': int,
    'f0': int, 'f1': int,
    'ctype0': int, 'ctype1': int,
    'mode': (1, 2, 3, 4),
    'version': int, 'version_esp': str, 'model': int,
    'voltage': float, 'rssi': int, 'period_min': int,
}

# Поля, которых на младшей прошивке нет и не должно быть. Требовать их со всех
# версий - значит красить в красный верное поведение: на 2.0.44 в посылке про
# тревоги, режим отпуска и маску квитанции нет ни слова, потому что и самих
# возможностей нет. Версии - по истории json.cpp, а не на глаз.
FIELDS_SINCE: dict[tuple[int, int, int], dict[str, Any]] = {
    (2, 0, 47): {
        'alarm_flow0': bool, 'alarm_flow1': bool,
        'alarm_leak0': bool, 'alarm_leak1': bool,
        'alarm_wet0': bool, 'alarm_wet1': bool,
        'alarm_stop0': bool, 'alarm_stop1': bool,
        'af0': int, 'af1': int, 'al0': int, 'al1': int, 'as0': int, 'as1': int,
        'vac': bool, 'sc': bool,
        'ackw': bool, 'ackh': bool, 'ackm': bool,
    },
}


def expected_fields(esp_version: tuple[int, int, int] | None) -> dict[str, Any]:
    fields = dict(REQUIRED_FIELDS)
    for since, group in FIELDS_SINCE.items():
        if esp_version is not None and esp_version >= since:
            fields.update(group)
    return fields


@pytest.mark.mqtt          # проверяет все три канала, включая брокер
@pytest.mark.requires(esp='2.0.47')       # вердикт читается из строки Alarm confirm
def test_G1_all_three_channels(stand: Stand) -> None:
    """Короткое нажатие: показания уходят во все три канала."""
    stand.reset_observers()
    stand.dut.press_button()

    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    session.assert_confirm(waterius=SEND_OK, http=SEND_OK, mqtt=SEND_OK)
    assert session.payload is not None, 'приёмник не получил посылку'

    # Успех не моргается ни на одной модели (main.cpp), значит и строки нет
    assert session.blynk is None, f'удачный сеанс собрался моргать: {session.blynk}'

    # Ловит дефект, при котором повторная отправка после применения настроек
    # уходит в брокер уже после disconnect
    assert 'MQTT: Not connected' not in session.text

    assert stand.mqtt is not None
    assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30) is not None, (
        f'в брокере нет ни одного топика {stand.mqtt_root}/, '
        f'пришло: {stand.mqtt.topics()}')


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

    fields = expected_fields(stand.esp_version)
    missing = [name for name in fields if name not in payload]
    assert not missing, f'в посылке нет полей: {missing}'

    for name, expected in fields.items():
        value = payload[name]
        if isinstance(expected, tuple):
            assert type(value) is not bool and value in expected, \
                f'{name}={value!r}, допустимо {expected}'
        elif expected is float:
            assert type(value) in (int, float), f'{name}={value!r} не число'
        else:
            # type(), а не isinstance(): bool - подкласс int, и обе проверки
            # прошли бы для любого из двух форматов флага
            assert type(value) is expected, f'{name}={value!r} не {expected.__name__}'


def test_G3_no_router(stand: Stand) -> None:
    """
    Роутера нет: устройство не подключилось и до отправки не дошло.

    Строки `Alarm confirm` в таком сеансе быть не должно - её печатают уже
    после подключения, и её появление означало бы, что прошивка считает
    отправку состоявшейся без сети.
    """
    stand.reset_observers()

    with stand.net.ap_off():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert not session.wifi_connected, session.text
    assert session.text.count('WIFI: Connection failed.') >= 2, session.text
    assert session.confirm is None, f'сеанс без сети, а получатели отчитались\n{session.text}'
    assert session.payload is None, 'приёмник не мог получить посылку без сети'
    assert session.blynk == BLYNK_ROUTER, session.text


@pytest.mark.requires(esp='2.0.47')
def test_G4a_cloud_unreachable(stand: Stand) -> None:
    """
    Облака waterius.ru нет, свой сервер жив.

    Требует 2.0.47: статусы получателей поимённо печатает строка
    `Alarm confirm`, которой на младших прошивках нет вовсе.
    """
    stand.reset_observers()

    with stand.net.waterius_down():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.wifi_connected, 'Wi-Fi должен был подняться: режем только трафик'
    session.assert_confirm(waterius=SEND_NO_CONNECTION, http=SEND_OK, any=1)
    assert session.payload is not None, 'свой сервер жив, посылка обязана дойти'
    assert session.blynk == BLYNK_CLOUD, session.text


@pytest.mark.requires(esp='2.0.47')
def test_G4b_own_server_unreachable(stand: Stand) -> None:
    """
    Своего сервера нет, облако живо. Зеркало предыдущего теста.

    Разница с ним - одно число в строке лога, и ради него всё и затевалось:
    по вспышкам эти два случая неразличимы (core/blink.h, merge_status), а
    чинить их надо по-разному.
    """
    stand.reset_observers()

    with stand.net.own_server_down():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.wifi_connected, 'Wi-Fi должен был подняться: режем только трафик'
    session.assert_confirm(waterius=SEND_OK, http=SEND_NO_CONNECTION, any=1)
    assert session.payload is None, 'приёмник стенда отрезан, посылки быть не должно'

    # Тот же код, что и у G4a: вспышками эти две поломки не различить, и
    # именно поэтому тесты смотрят на статусы получателей, а не на код
    assert session.blynk == BLYNK_CLOUD, session.text


@pytest.mark.mqtt
@pytest.mark.requires(esp='2.0.47')       # младшие не печатают MQTT: Connecting failed
def test_G5_broker_unreachable(stand: Stand) -> None:
    """
    Брокер недоступен, облако живо.

    Проверяются обе строки: `MQTT: Connect failed with state` печатается на
    каждой попытке, `MQTT: Connecting failed` - один раз, когда попытки
    исчерпаны. Без второй строки сеанс по логу выглядит так, будто
    подключение удалось.
    """
    stand.reset_observers()

    with stand.net.mqtt_down():
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.blynk == BLYNK_MQTT, session.text
    assert 'MQTT: Connect failed with state' in session.text
    assert 'MQTT: Connecting failed' in session.text
    assert session.confirm and session.confirm['waterius'] == SEND_OK


@pytest.mark.requires(esp='2.0.47')
def test_G7_server_answers_500(stand: Stand) -> None:
    """
    Свой сервер отвечает 500: сеть в порядке, данные не приняты.

    Успехом прошивка считает только 200 (`https_helpers.cpp`), поэтому любой
    другой код - это «сервер ответил не то». От недоступного сервера случай
    отличается двумя числами: в `Alarm confirm` статус 2, а не 3, и код
    вспышек 6, а не 3. Чинить их надо по-разному: там сеть и адрес, здесь
    токен, тариф или сам сервер.
    """
    stand.reset_observers()

    with stand.receiver.answering(500):
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.wifi_connected, 'сеть цела: портится только ответ сервера'
    session.assert_confirm(waterius=SEND_OK, http=SEND_BAD_ANSWER, any=1)
    assert session.blynk == BLYNK_CLOUD_ANSWER, session.text

    # Прошивка повторяет отправку HTTP_SEND_ATTEMPTS раз (sender_http.h) и
    # каждый раз получает тот же код: приёмник посылку принял, а прошивка
    # считает её недоставленной - в этом весь сценарий
    assert session.http_codes.count(500) == HTTP_SEND_ATTEMPTS, session.http_codes
    assert len(session.payloads) == HTTP_SEND_ATTEMPTS, (
        f'посылок дошло {len(session.payloads)}, попыток {HTTP_SEND_ATTEMPTS}')


@pytest.mark.requires(esp='2.0.47')
def test_G8_own_server_over_https(stand: Stand) -> None:
    """
    Свой сервер по https с самоподписанным сертификатом.

    Сертификат прошивка не проверяет (`https_helpers.cpp`, setInsecure), и это
    проверяется именно так, как работает у пользователя: адрес по ip, имя в
    сертификате - тот же ip. Доказательство доставки - счётчик приёмника, а не
    строка лога: `HTTP: Create secure client` печатает и облако.
    """
    stand.receiver.start_tls(stand.cfg.receiver_tls_port)
    before = stand.receiver.tls_hits
    try:
        stand.setup(http_url=stand.cfg.https_url)
        stand.reset_observers()
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        session.assert_confirm(http=SEND_OK, any=1)
        assert session.payload is not None, 'посылка не дошла'
        assert stand.receiver.tls_hits > before, (
            'посылка пришла, но не по https - адрес не сменился')
    finally:
        stand.setup(http_url=stand.cfg.http_url)
