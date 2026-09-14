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

from .constants import HTTP_SEND_ATTEMPTS, PLANNED_PERIOD_MIN, PLANNED_WAIT_S
from .logwatch import (BLYNK_CLOUD, BLYNK_CLOUD_ANSWER, BLYNK_MQTT,
                       BLYNK_ROUTER, MANUAL_TRANSMIT_MODE, SEND_BAD_ANSWER,
                       SEND_NO_CONNECTION, SEND_OK, SEND_SKIPPED, TRANSMIT_MODE)
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .logwatch import Session  # а сбор тестов должен работать без них
    from .stand import Stand

pytestmark = pytest.mark.stand

# Поля, которые обязаны быть в каждой посылке. Проверяем не только наличие, но
# и тип с диапазоном: поле, ставшее всегда нулевым, список имён не поймает.
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
        'av0': int, 'av1': int, 'ar0': int, 'ar1': int,
        'ah0': int, 'ah1': int, 'as0': int, 'as1': int,
        'vac': bool, 'sc': bool,
        'ackw': bool, 'ackh': bool, 'ackm': bool,
    },
}


# Правдоподобные границы. Ловят класс #22 и #269: беззнаковый перенос давал
# в показаниях и приросте 42949670 и 4.29e9 при верных типах
RANGES: dict[str, tuple[float, float]] = {
    'ch0': (0, 1e6), 'ch1': (0, 1e6),
    'delta0': (0, 1e6), 'delta1': (0, 1e6),
    'imp0': (0, 1e9), 'imp1': (0, 1e9),
    'voltage': (2.0, 5.5),
    'rssi': (-100, 0),
    'period_min': (1, 65535),
}

# Период из ответа сервера в G1b: любой, отличный от эталона стенда
RESEND_PERIOD_MIN = 95


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

    assert stand.mqtt is not None
    assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30) is not None, (
        f'в брокере нет ни одного топика {stand.mqtt_root}/, '
        f'пришло: {stand.mqtt.topics()}')


@pytest.mark.mqtt
@pytest.mark.requires(esp='2.0.47')
@pytest.mark.needs(mqtt_auto_discovery=1)     # показания одним объектом в корень
def test_G1b_resend_after_settings_reaches_broker(stand: Stand) -> None:
    """
    Настройки в ответе сервера: повторная посылка того же сеанса доходит и до
    брокера (#406).

    Повторная отправка бывает только в сеансе, где применили настройки
    (main.cpp, второй send_data), поэтому сеанс без них дефекта не видит.
    Корень сравнивается со второй посылкой: при закрытом соединении в брокере
    осталась бы первая, со старым периодом.
    """
    assert stand.mqtt is not None
    stand.reset_observers()
    stand.receiver.reply_settings({'period_min': RESEND_PERIOD_MIN})
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert session.applied.get('period_min') == str(RESEND_PERIOD_MIN), session.applied
    assert len(session.payloads) >= 2, 'после применения данные должны уйти повторно'
    assert 'MQTT: Not connected' not in session.text, session.text
    session.assert_confirm(mqtt=SEND_OK)

    assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=10) is not None
    message = stand.mqtt.last(stand.mqtt_root)
    assert message is not None, (
        f'в корне ничего нет, дерево: {stand.mqtt.topics(stand.mqtt_root)}')
    assert message.json()['period_min'] == RESEND_PERIOD_MIN, (
        'в брокере осталась первая посылка сеанса: повторная до него не дошла')


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

    for name, (low, high) in RANGES.items():
        assert low <= payload[name] <= high, f'{name}={payload[name]!r} вне [{low}, {high}]'


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
    stand.setup(http_url=stand.cfg.https_url)
    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_confirm(http=SEND_OK, any=1)
    assert session.payload is not None, 'посылка не дошла'
    assert stand.receiver.tls_hits > before, (
        'посылка пришла, но не по https - адрес не сменился')


@pytest.mark.mqtt
@pytest.mark.requires(esp='2.0.47')
def test_G9_hanging_server(stand: Stand) -> None:
    """
    Свой сервер принял соединение и молчит (#367).

    Не то же, что G4b: там порт закрыт и отказ приходит сразу, здесь прошивка
    ждёт своего таймаута на каждой попытке. Утверждается исход, а не время:
    сеанс доигран до сна, облако и брокер своё получили, свой сервер - нет.
    Не уложись попытки в окно attiny (`WAIT_ESP_MSEC`), питание сняли бы
    посреди сеанса, и строки `Going to sleep` не было бы.
    """
    stand.reset_observers()
    hung = stand.receiver.hung

    with stand.receiver.hanging():
        stand.dut.press_button()
        session = stand.wait_session(timeout=300, mode=MANUAL_TRANSMIT_MODE)

    assert stand.receiver.hung > hung, 'прошивка не дошла до своего сервера'
    assert session.complete, f'сеанс оборван до сна\n{session.text}'
    session.assert_confirm(waterius=SEND_OK, mqtt=SEND_OK)
    assert session.confirm['http'] != SEND_OK, session.confirm
    assert session.payload is None, 'сервер не ответил, посылки в очереди быть не должно'


# Получатели по именам параметров прошивки (portal/resources.h)
RECEIVERS = ('waterius_on', 'http_on', 'mqtt_on')


def switch_receivers(stand: Stand, **flags: int) -> Session:
    """
    Включить и выключить получателей ответом приёмника.

    `stand.setup` здесь не годится: он ждёт повторной посылки на приёмник, а
    выключенный свой сервер её не получит. Свидетельство - строки `Saved:`.
    """
    stand.reset_observers()
    stand.receiver.reply_settings(flags)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    for name, value in flags.items():
        assert session.saved.get(name) == str(value), (
            f'{name} не сохранён: {session.saved}\n{session.text}')
    return session


def restore_receivers(stand: Stand, alone: str) -> None:
    """Вернуть всех трёх получателей через того, кто остался включён."""
    if alone == 'http_on':
        switch_receivers(stand, **{name: 1 for name in RECEIVERS})
        return

    assert stand.mqtt is not None
    stand.reset_observers()
    for name in RECEIVERS:
        stand.mqtt.publish_set(name, 1, retain=True)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    left = {name: session.saved.get(name) for name in RECEIVERS
            if session.saved.get(name) != '1'}
    assert not left, f'получатели не вернулись командой MQTT: {left}\n{session.text}'


@pytest.mark.mqtt
@pytest.mark.requires(esp='2.0.47')
@pytest.mark.parametrize('alone', ['http_on', 'mqtt_on'], ids=['only_http', 'only_mqtt'])
def test_G10_single_receiver(stand: Stand, alone: str) -> None:
    """
    Включён один получатель - данные уходят ему, остальные пропущены (#320).

    #320: при выключенном waterius.ru переставали уходить данные в MQTT.
    Правило маршрутов проверяет хостовый test_routing, здесь - сеанс целиком.
    Случая «только облако» нет: свой сервер и брокер тогда вернуть стенду
    можно лишь порталом.
    """
    assert stand.mqtt is not None
    flags = {name: int(name == alone) for name in RECEIVERS}
    if alone == 'mqtt_on':
        # Вернуть остальных можно будет только командой, а команды слушает
        # лишь устройство с автодискавери (I4c)
        flags['mqtt_auto_discovery'] = 1

    try:
        switch_receivers(stand, **flags)

        stand.reset_observers()
        stand.dut.press_button()
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

        session.assert_confirm(**{name.removesuffix('_on'): SEND_OK if name == alone
                                  else SEND_SKIPPED for name in RECEIVERS})
        assert session.blynk is None, f'выключенный получатель - не ошибка\n{session.text}'
        if alone == 'http_on':
            assert session.payload is not None, 'свой сервер включён, а посылки нет'
        else:
            assert session.payload is None, 'свой сервер выключен, а посылка пришла'
            assert stand.mqtt.wait_prefix(stand.mqtt_root, timeout=30) is not None, (
                f'брокер включён один, а в {stand.mqtt_root}/ пусто')
    finally:
        restore_receivers(stand, alone)


@pytest.mark.slow
@pytest.mark.needs(period_min=PLANNED_PERIOD_MIN)
def test_G11_router_reboot_between_sessions(stand: Stand) -> None:
    """
    Роутер перезагрузился между сеансами - плановый сеанс снова в сети (#204).

    Плановый, а не по кнопке: в #204 устройство переставало выходить на связь
    само и оживало только от нажатия.
    """
    stand.router.restart()
    stand.reset_observers()

    session = stand.wait_session(timeout=PLANNED_WAIT_S, mode=TRANSMIT_MODE)

    assert session.wifi_connected, f'после перезагрузки роутера сети нет\n{session.text}'
    assert session.payload is not None, 'сеанс в сети, а посылки нет'
