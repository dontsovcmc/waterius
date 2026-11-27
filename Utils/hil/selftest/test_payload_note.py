"""
Отказ «посылки нет» называет причину словами устройства.

Прогон 30 сентября: G2 упал голым `assert None is not None`, W1 - словами
«устройство не прислало показания». В обоих сеансах устройство дважды не
подключилось к Wi-Fi, стенд это знал и в отказ не положил, и отказ читался как
дефект отправки.
"""

from __future__ import annotations

import pytest

from .test_logwatch import fw
from .test_wifi_verdict import НЕ_ПОДКЛЮЧИЛСЯ, ПОДКЛЮЧИЛСЯ, РАДИО_НЕ_ВКЛЮЧАЛИ, сеанс

ТАЙМАУТ = [
    fw('Startup mode: 3'),
    fw('WIFI: Attempt #1 from 2'),
    fw('WIFI: connect timeout', 'ERROR'),
    fw('WIFI: Connection failed.', 'ERROR'),
    fw('WIFI: Attempt #2 from 2'),
    fw('WIFI: connect timeout', 'ERROR'),
    fw('WIFI: Connection failed.', 'ERROR'),
    fw('Going to sleep'),
]

СЕРВЕР_МОЛЧИТ = [
    fw('Startup mode: 3'),
    fw('WIFI: Attempt #1 from 2'),
    fw('WIFI: Connected.'),
    fw('HTTP: Send new data'),
    fw('HTTP: Response code: -1'),
    fw('HTTP: Failed send data. Time 36443 ms'),
    fw('Alarm confirm: mask=0 waterius=1 http=3 mqtt=1 any=1 -> 1'),
    fw('Going to sleep'),
]


def test_посылка_есть_отказа_нет() -> None:
    session = сеанс(ПОДКЛЮЧИЛСЯ)
    session.payload = {'ch0': 1.0}
    assert session.expect_payload() == {'ch0': 1.0}


def test_отказ_wifi_назван_отказом_wifi() -> None:
    with pytest.raises(AssertionError) as отказ:
        сеанс(ТАЙМАУТ).expect_payload('после мастера устройство не прислало показания')
    текст = str(отказ.value)
    assert текст.startswith('после мастера устройство не прислало показания: '
                            'устройство не подключилось к сети (2 попыток')
    # Повторы одной ошибки - одной строкой: число попыток уже сказано
    assert 'WIFI: connect timeout; WIFI: Connection failed.' in текст
    assert 'отказ Wi-Fi, а не отправки' in текст
    # Строки устройства - сама улика
    assert 'WIFI: Attempt #2 from 2' in текст


def test_отказ_wifi_без_строк_ошибок() -> None:
    assert 'без строк ошибок' not in сеанс(НЕ_ПОДКЛЮЧИЛСЯ).no_payload_note
    без_ошибок = [line for line in НЕ_ПОДКЛЮЧИЛСЯ if 'ERROR' not in line]
    assert 'без строк ошибок' in сеанс(без_ошибок).no_payload_note


def test_в_сети_называет_ответ_своего_сервера() -> None:
    note = сеанс(СЕРВЕР_МОЛЧИТ).no_payload_note
    assert note == 'устройство в сети, свой сервер ответил [-1], итог получателя http=3'


def test_в_сети_без_отправки_на_свой_сервер() -> None:
    assert 'кодов его ответа в сеансе нет' in сеанс(ПОДКЛЮЧИЛСЯ).no_payload_note


def test_оборванное_тело_приписано_эфиру() -> None:
    session = сеанс(СЕРВЕР_МОЛЧИТ)
    session.broken = 1
    assert '05_air-and-loss.md' in session.no_payload_note


def test_сеанс_без_радио() -> None:
    assert 'радио в этом сеансе не включалось' in сеанс(РАДИО_НЕ_ВКЛЮЧАЛИ).no_payload_note
