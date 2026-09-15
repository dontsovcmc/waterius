"""
Проверка ввода в портале - блок C ручного плана.

Отвергает значение прошивка, а не разметка: страница пропускает четыре цифры в
поле часов и любую строку в поле адреса, а отказ приходит от `/api/save`
кодом ошибки в JSON (`active_point_api.cpp`, report_param_error). Значит и
проверять надо прошивку - AT-платой, без браузера.

Коды - из `core/input.h`: 14 длина, 15 значение, 19 забыта запятая, 20 TLS,
21 порт в адресе. Отвергнутый параметр не сохраняется, поэтому опыты здесь
безопасны: адрес брокера и показания остаются прежними.

Поле из одних звёздочек (C6) - в `test_settings.py`. Прошивка распознаёт его
как «не редактировали» и оставляет прежний пароль, а прочитать пароль для
сверки неоткуда: портал отдаёт те же звёздочки. Проверять приходится
следствием - следующий сеанс снова в сети, - а для этого надо выйти из
портала, который здесь открыт на весь модуль.
"""

from __future__ import annotations

import json
import time
from typing import Any, Iterator
from urllib.parse import urlencode

import pytest

from . import portal as portal_mod
from .atboard import AtBoard
from .constants import (ALARM_STOP_MAX_HOURS, ERR_LENGTH, ERR_NO_COMMA,
                        ERR_PORT_IN_HOST, ERR_TLS, ERR_VALUE, SERIAL_LEN)

pytestmark = [pytest.mark.stand, pytest.mark.portal]


@pytest.fixture(scope='module')
def board(cfg: Any, stand: Any) -> Iterator[AtBoard]:
    """Портал на весь модуль: проверки ввода железа не трогают."""
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    with portal_mod.session(cfg, stand) as device:
        yield device


def save(board: AtBoard, path: str, **params: Any) -> dict[str, str]:
    """
    Отправить параметры в портал и вернуть ошибки полей.

    Параметры уходят телом, как их шлёт форма. Строкой запроса нельзя:
    `/api/save_alarms` читает поля через `hasParam(name, true)`, то есть
    только из тела, и параметр из строки молча пропадёт вместе с проверкой.
    """
    answer = board.post(path, portal_mod.HOST, body=urlencode(params).encode())
    assert answer.status == 200, f'{path}: {answer.status}'
    body = json.loads(answer.text)
    return {name: str(code) for name, code in (body.get('errors') or {}).items()}


def test_C1_water_readings_need_liters(board: AtBoard) -> None:
    """Показания воды без разделителя - «вводятся с литрами»."""
    assert save(board, '/api/save', input=1,
                channel_start='123456') == {'channel_start': ERR_NO_COMMA}


def test_C2_broker_over_tls_is_refused(board: AtBoard) -> None:
    """mqtts:// - шифрованное подключение к брокеру не поддерживается."""
    assert save(board, '/api/save', mqtt_on=1,
                mqtt_host='mqtts://broker') == {'mqtt_host': ERR_TLS}


def test_C3_port_belongs_to_its_own_field(board: AtBoard) -> None:
    """Порт в адресе брокера - отдельная ошибка, а не «неверное значение»."""
    assert save(board, '/api/save', mqtt_on=1,
                mqtt_host='broker:1883') == {'mqtt_host': ERR_PORT_IN_HOST}


def test_C4_stop_threshold_has_a_ceiling(board: AtBoard) -> None:
    """
    Остановка расхода больше потолка отвергается.

    Через интерфейс это ловится только так: четыре цифры проходят проверку
    разметки, а отвергает значение прошивка (`save_stop_param`).
    """
    key = 'alarm_stop1'
    try:
        assert save(board, '/api/save_alarms',
                    **{key: ALARM_STOP_MAX_HOURS + 1}) == {key: ERR_VALUE}
        # Граница снизу: потолок принимается, то есть отвергнуто именно лишнее
        assert save(board, '/api/save_alarms', **{key: ALARM_STOP_MAX_HOURS}) == {}
    finally:
        assert save(board, '/api/save_alarms', **{key: 0}) == {}


def test_C5_length_is_counted_in_bytes(board: AtBoard) -> None:
    """
    Длина поля - в байтах, а не в символах: кириллица занимает по два.

    Девять букв против шестнадцати байт: по символам поле бы влезло.
    """
    try:
        assert save(board, '/api/save', input=0,
                    serial='ы' * (SERIAL_LEN // 2 + 1)) == {'serial': ERR_LENGTH}
        # Та же длина в символах, но байт вдвое меньше - принимается
        assert save(board, '/api/save', input=0,
                    serial='A' * (SERIAL_LEN // 2 + 1)) == {}
    finally:
        assert save(board, '/api/save', input=0, serial='') == {}


def test_C7_disabled_transport_is_not_parsed(board: AtBoard) -> None:
    """
    Поля выключенного транспорта не разбираются: заведомо битый адрес брокера
    сохраняется без ошибки, потому что не сохраняется вовсе.
    """
    try:
        assert save(board, '/api/save', mqtt_on=0, mqtt_host='mqtts://broker') == {}
    finally:
        # Брокер обязан вернуться даже после падения: выключенным его
        # унаследует весь блок I
        assert save(board, '/api/save', mqtt_on=1) == {}


def test_C8_valid_forms_are_accepted(board: AtBoard, cfg: Any, stand: Any) -> None:
    """
    Зеркало C1-C3: верное значение принимается - с запятой, с пятью целыми
    разрядами, со схемой и путём в адресе брокера (#313, #330, #353).

    Отказ здесь человек увидел бы так же, как в C1-C3, но это была бы ошибка
    прошивки. Адрес - брокера стенда: разбор обязан снять схему и путь, и брокер
    остаётся прежним. Показания возвращаются последними известными стенду.
    """
    last = stand.last_payload or {}
    try:
        assert save(board, '/api/save', input=1, channel_start='12,345') == {}
        assert save(board, '/api/save', input=1, channel_start='99999.999') == {}
        assert save(board, '/api/save', mqtt_on=1,
                    mqtt_host=f'mqtt://{cfg.broker_host}/stand') == {}
    finally:
        if 'ch1' in last:
            assert save(board, '/api/save', input=1,
                        channel_start=f"{float(last['ch1']):.3f}") == {}


def saved_line(stand: Any, value: str, timeout: float = 5.0) -> bool:
    """Есть ли в логе строка `Saved` с этим значением: ответ портала пуст в обоих случаях."""
    deadline = time.time() + timeout
    while True:
        stand.log.poll()
        if any('Saved' in line and value in line for line in stand.log.lines):
            return True
        if time.time() >= deadline:
            return False
        time.sleep(0.5)


QUERY_MARK = 'QSTRING'


def test_C10_settings_come_only_from_the_body(board: AtBoard, stand: Any) -> None:
    """
    Настройка из строки запроса не сохраняется (PR #431, #440).

    Иначе ссылка `/api/save?serial=...`, открытая в сети портала, меняла бы
    настройки без формы. Признак маршрута (input) из строки читается - он
    ничего не сохраняет (`active_point_api.cpp`, from_form).

    Контроль - то же значение телом: без него тест зеленел бы и на прошивке,
    которая серийный номер не сохраняет вовсе.
    """
    stand.log.clear()
    answer = board.post(f'/api/save?input=0&serial={QUERY_MARK}', portal_mod.HOST)
    assert answer.status == 200, answer.status
    assert not saved_line(stand, QUERY_MARK), 'значение из строки запроса сохранено'

    try:
        assert save(board, '/api/save', input=0, serial=QUERY_MARK) == {}
        assert saved_line(stand, QUERY_MARK), 'телом значение не сохранилось - контроль сломан'
    finally:
        assert save(board, '/api/save', input=0, serial='') == {}
