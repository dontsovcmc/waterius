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

from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .logwatch import Session  # а сбор тестов должен работать без них
    from .stand import Stand

pytestmark = pytest.mark.stand

# portal/resources.h: PARAM_ASTERICS - так интерфейс показывает сохранённый пароль
MASKED = '********'


def apply(stand: Stand, settings: dict[str, Any]) -> Session:
    """Отдать настройки ответом приёмника и вернуть сеанс, который их применил."""
    stand.reset_observers()
    stand.receiver.reply_settings(settings)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    missing = [name for name in settings if name not in session.applied]
    assert not missing, (
        f'прошивка не применила {missing}; в логе: {session.applied}\n{session.text}')
    assert len(session.payloads) >= 2, 'после применения данные должны уйти повторно'
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
