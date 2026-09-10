"""
Проверка ввода в портале - блок C ручного плана.

Отвергает значение прошивка, а не разметка: страница пропускает четыре цифры в
поле часов и любую строку в поле адреса, а отказ приходит от `/api/save`
кодом ошибки в JSON (`active_point_api.cpp`, report_param_error). Значит и
проверять надо прошивку - AT-платой, без браузера.

Коды - из `core/input.h`: 14 длина, 15 значение, 19 забыта запятая, 20 TLS,
21 порт в адресе. Отвергнутый параметр не сохраняется, поэтому опыты здесь
безопасны: адрес брокера и показания остаются прежними.

Чего здесь нет: случая с полем из одних звёздочек (C6). Прошивка распознаёт
его как «не редактировали» и молча оставляет прежнее значение, а прежнее
значение - пароль, и прочитать его для сверки неоткуда: портал отдаёт те же
звёздочки.
"""

from __future__ import annotations

import json
from typing import Any, Iterator
from urllib.parse import urlencode

import pytest

from . import portal as portal_mod
from .atboard import AtBoard

pytestmark = [pytest.mark.stand, pytest.mark.portal]

ERR_LENGTH = '14'
ERR_VALUE = '15'
ERR_NO_COMMA = '19'
ERR_TLS = '20'
ERR_PORT_IN_HOST = '21'

# core/idle.h: потолок остановки расхода в часах
ALARM_STOP_MAX_HOURS = 1092

# core/types.h: SERIAL_LEN. Девять кириллических букв - это 18 байт, то есть
# длина считается в байтах, а не в символах
SERIAL_LEN = 16


@pytest.fixture(scope='module')
def board(cfg: Any, stand: Any) -> Iterator[AtBoard]:
    """Портал на весь модуль: проверки ввода железа не трогают."""
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    with portal_mod.session(cfg, stand) as device:
        yield device


def save(board: AtBoard, path: str, **params: Any) -> dict[str, str]:
    """Отправить параметры в портал и вернуть ошибки полей."""
    answer = board.post(f'{path}?{urlencode(params)}', portal_mod.HOST)
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
