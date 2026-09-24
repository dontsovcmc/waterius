"""
Настройки, присланные извне, - блок S ручного плана и C6.

Сервер и Home Assistant шлют настройки строками, и прошивка разбирает их теми
же правилами, что и форму портала: `apply_settings` (`ha/apply_settings.cpp`)
раскладывает ключи по `applyInputParameter`, `applyCheckBoxParameter` и
`applyNonCheckBoxParameter`, а те зовут разбор из `core/input.h`. Проверяется
то, что уже ломалось: запятая в показаниях (#313, #332), флажки словами
(#419), пробелы по краям (#224), звёздочки вместо пароля (C6).

`stand.setup` здесь не годится: он сверяет присланное с посылкой через
`same_value`, а тот считает `'false'` истиной и `' SN '` не равным `'SN'`.
Проверка разбора - это как раз сверка строки с тем, во что она превратилась.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import pytest

from .constants import MASKED
from .logwatch import MANUAL_TRANSMIT_MODE
from .state import same_value

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .logwatch import Session  # а сбор тестов должен работать без них
    from .stand import Stand

pytestmark = pytest.mark.stand


def apply(stand: Stand, settings: dict[str, Any]) -> Session:
    """Отдать настройки ответом приёмника и вернуть сеанс, который их применил."""
    stand.reset_observers()
    stand.receiver.reply_settings(settings)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    missing = [name for name in settings if name not in session.applied]
    assert not missing, (
        f'прошивка не применила {missing}; в логе: {session.applied}\n{session.text}')
    assert len(session.payloads) >= 2, (
        'после применения данные должны уйти повторно' + session.air_note)
    assert session.payload is not None
    return session


def test_S1_readings_accept_a_comma(stand: Stand) -> None:
    """
    Запятая в показаниях - такой же разделитель, как точка (#313, #332).

    Её ставит любой, кто пишет по-русски, и отказ выглядел бы для него
    необъяснимым. Разбор - `parse_decimal`: запятая и точка равноправны.
    """
    session = apply(stand, {'ch1': '12,345'})
    assert abs(float(session.payload['ch1']) - 12.345) < 0.001, (
        f"ждали 12.345, приехало {session.payload['ch1']}")


def test_S2_flags_accept_words(stand: Stand) -> None:
    """
    Флажок принимает true/false словами и в любом регистре (#419).

    Посылка отдаёт флаги как true/false (`json.cpp`), и значение, прочитанное
    из неё, должно возвращаться обратно настройкой как есть. Флажок выбран
    безвредный: бит квитанции waterius.ru, в эталоне он включён.
    """
    off = apply(stand, {'ackw': 'false'})
    assert off.payload['ackw'] is False, f"ackw={off.payload['ackw']!r}"

    on = apply(stand, {'ackw': 'TRUE'})
    assert on.payload['ackw'] is True, f"ackw={on.payload['ackw']!r}"


def test_S3_text_is_trimmed(stand: Stand) -> None:
    """
    Пробелы по краям текстового поля обрезаются (#224), серийный номер
    доезжает до посылки (#181).

    Пробел, скопированный вместе с номером из квитанции, иначе стал бы частью
    номера, и сверка с данными управляющей компании не сошлась бы.
    """
    try:
        session = apply(stand, {'serial1': '  SN-181  '})
        assert session.payload['serial1'] == 'SN-181', (
            f"serial1={session.payload['serial1']!r}")
    finally:
        apply(stand, {'serial1': ''})


def test_C6_masked_password_keeps_the_old_one(stand: Stand) -> None:
    """
    Пароль из одних звёздочек не затирает сохранённый.

    Так интерфейс показывает пароль, который не меняли (P5), и форма
    возвращает его обратно звёздочками. Прошивка обязана их узнать
    (`core/input.h`, is_all_asterisks) и оставить прежний пароль: иначе каждое
    сохранение страницы Wi-Fi меняло бы его на восемь звёздочек, и устройство
    теряло бы сеть на следующем же сеансе.

    Прочитать пароль для сверки неоткуда - портал отдаёт те же звёздочки,
    поэтому проверка - следствием: следующий сеанс снова в сети. Параметр идёт
    тем же обработчиком, что из формы (`applyNonCheckBoxParameter`, ветка
    password).
    """
    apply(stand, {'password': MASKED})

    stand.reset_observers()
    stand.dut.press_button()
    try:
        session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert session.wifi_connected and session.payload is not None, (
            f'после звёздочек устройство не в сети: пароль затёрт\n{session.text}')
    except Exception:
        # Без пароля весь прогон дальше шёл бы без сети: возвращаем его порталом
        stand.ensure_network(force=True)
        raise


# Значения, которые прошивка обязана отвергнуть. Разбор общий с порталом
# (core/input.h), а путь применения свой: ответ сервера и команда HA идут
# через apply_settings, минуя обработчики страниц портала
INVALID = [
    pytest.param('period_min', '0', id='period_zero'),
    pytest.param('period_min', 'abc', id='period_text'),
    pytest.param('period_min', '70000', id='period_overflow'),    # сохранялось 4464 (PR #378)
    pytest.param('ch1', 'abc', id='water_reading_text'),
    pytest.param('f1', '-1', id='factor_negative'),
    pytest.param('ctype1', '3', id='ctype_unknown', marks=pytest.mark.xfail(
        strict=True,
        reason='тип входа из ответа сервера и из HA уходит в attiny без '
               'is_valid_counter_type (active_point_api.cpp, applyInputParameter): '
               'проверку делает только страница портала')),
]


@pytest.mark.parametrize('name, value', INVALID)
def test_S4_invalid_value_is_rejected(stand: Stand, name: str, value: str) -> None:
    """
    Негодное значение с сервера отвергается: настройка прежняя, сеанс доигран.

    Сверка - с посылкой до применения: стенд помнит последнюю, и в ней то,
    что устройство думает о себе сейчас.
    """
    before = stand.last_payload
    assert before is not None and name in before, f'в посылке нет {name}'

    session = apply(stand, {name: value})

    assert same_value(session.payload[name], before[name]), (
        f'{name}={value!r} применено: было {before[name]!r}, '
        f'стало {session.payload[name]!r}')


@pytest.mark.xfail(strict=True, reason=(
    'reset_period_min_tuned зовётся и для отвергнутого period_min '
    '(active_point_api.cpp, applyNonCheckBoxParameter)'))
def test_S4b_rejected_period_keeps_tuning(stand: Stand) -> None:
    """
    Отвергнутый период не сбрасывает подстройку хода attiny.

    Сброс (`config.cpp`, reset_period_min_tuned) обнуляет поправку, прогрев
    NTP и измеренный полный период - оправдан он только настоящей сменой
    периода. Свидетельство - строка `RESET: period_min_tuned`: числом в
    посылке сброс виден не всегда, после свежей настройки период и так
    равен сброшенному.
    """
    session = apply(stand, {'period_min': '0'})
    assert 'RESET: period_min_tuned' not in session.text, session.text


@pytest.mark.requires(esp='2.0.50')
def test_C6b_masked_password_keeps_fast_connect(stand: Stand) -> None:
    """
    Звёздочки вместо пароля не сбрасывают быстрый коннект.

    Пароль прежний (C6), значит и сеть прежняя: запомненные канал и BSSID
    сбрасывать незачем, а без них следующий сеанс идёт полным сканом.
    """
    apply(stand, {'password': MASKED})

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    assert session.wifi_connected, session.text
    assert 'WIFI: begin channel:' in session.text, (
        f'первая попытка - полным сканом: кэш сброшен\n{session.text}')
