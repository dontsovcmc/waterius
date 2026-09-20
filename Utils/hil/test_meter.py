"""
Показания и приросты - блок D ручного плана.

Период на время этих тестов длинный: при коротком плановое пробуждение
приходится ровно посреди серии импульсов, и прирост разъезжается на две
посылки. Это не дефект прошивки, а неверно поставленный опыт.
"""

from __future__ import annotations

import threading
import time
from typing import TYPE_CHECKING, Any

import pytest

from . import portal as portal_mod
from .constants import (BASE_FACTOR, COLD, ELECTRO, ELECTRONIC,
                        ELECTRONIC_HIGH, NAMUR)
from .logwatch import MANUAL_TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

# Столько, чтобы прирост был заведомо не нулевым и не совпал ни с числом
# импульсов, ни с весом: 3 x 10 = 30 литров. Больше не доказывает ничего -
# арифметика в прошивке одна и та же на трёх импульсах и на трёхстах.
PULSES = 3

# Импульс электронного выхода короткий: у газового счётчика Гранд 0,7-1,5 мс.
# Миллисекунда - предел стенда: выдержку отмеряет плата, а заказывается она в
# миллисекундах
SHORT_PULSE_MS = 1

# Положительный импульс стенд отмеряет с компьютера, поэтому он длинный.
# Длину проверяет соседний тест, здесь важна полярность
HIGH_PULSE_MS = 30


@pytest.mark.slow
def test_D2a_missed_session_keeps_consumption(stand: Stand) -> None:
    """
    Пропущенный сеанс не теряет расход.

    Точка отсчёта двигается в конце сеанса, а он выполняется только при
    поднятом Wi-Fi. Значит при выключенной точке доступа импульсы обоих
    периодов приедут одной посылкой.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    with stand.net.ap_off():
        stand.dut.pulse(channel=1, count=PULSES)
        stand.dut.press_button()
        missed = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert not missed.wifi_connected

    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=2 * PULSES * BASE_FACTOR)


@pytest.mark.slow
@pytest.mark.requires(esp='2.0.47')       # младшие двигают точку отсчёта после коннекта
def test_D2b_undelivered_session_keeps_delta(stand: Stand) -> None:
    """
    Тот же опыт, но сеть жива, а получатели недоступны.

    Прирост не теряется от того, что посылка не доехала: точка отсчёта
    двигается после доставки, а не после подключения к Wi-Fi.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    with stand.net.internet_down():
        stand.dut.pulse(channel=1, count=PULSES)
        stand.dut.press_button()
        missed = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert missed.wifi_connected, 'Wi-Fi должен был подняться'

    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=2 * PULSES * BASE_FACTOR)


@pytest.mark.parametrize('channel', [0, 1], ids=['red_input0', 'blue_input1'])
def test_D3_electricity_counts_kilowatt_hours(stand: Stand, channel: int) -> None:
    """
    У электричества вес импульса - это импульсы на кВт*ч, то есть делитель, а
    не множитель (`core/readings.cpp`). Десять импульсов при весе 10 дают ровно
    один киловатт-час.

    На обоих входах: #333 - электросчётчик на красном входе насчитывал
    миллионы, пока синий считал верно.

    Дробная часть прироста при этом теряется: `delta` уезжает в посылку целым
    числом. Проверяем именно так, как оно есть, - на целом киловатт-часе.
    """
    per_kwh = 10
    before = stand.setup(channel=channel, cname=ELECTRO, ctype=NAMUR,
                         factor=per_kwh, value='100')
    assert before.payload is not None
    kwh_before = float(before.payload[f'ch{channel}'])
    imp_before = int(before.payload[f'imp{channel}'])

    stand.reset_observers()
    stand.dut.pulse(channel=channel, count=per_kwh)
    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Счётчик импульсов attiny приезжает в той же посылке (`json.cpp`, imp0), и
    # без него расхождение в киловатт-часах ничего не говорит: непонятно, то ли
    # прошивка посчитала не так, то ли импульс не дошёл до входа. Прогон 20.09
    # дал ровно такой немой отчёт - 100,9 вместо 101,0, и причина осталась
    # неизвестной
    got = float(session.payload[f'ch{channel}'])
    counted = int(session.payload[f'imp{channel}']) - imp_before
    assert counted == per_kwh, (
        f'до входа дошло {counted} импульсов из {per_kwh}: считает не прошивка, '
        f'а стенд не довёл импульс. Показания: было {kwh_before}, стало {got}')
    assert abs(got - kwh_before - 1.0) < 0.001, (
        f'было {kwh_before}, стало {got}; импульсов attiny насчитал {counted}')
    session.assert_delta(channel=channel, liters=1)


def test_D4_readings_below_current_are_accepted(stand: Stand) -> None:
    """
    Показания меньше текущих: точка отсчёта переписывается, показания не уходят
    в минус.

    Так выглядит и обычная ошибка ввода, и замена счётчика на новый с нулём на
    табло: прошивка обязана поверить человеку, а не своей истории.
    """
    stand.setup(channel=1, ctype=NAMUR, factor=BASE_FACTOR, value='500.000')
    lowered = stand.setup(channel=1, value='1.000')

    assert lowered.payload is not None
    assert abs(float(lowered.payload['ch1']) - 1.0) < 0.001, (
        f"показания не сбросились: {lowered.payload['ch1']}")

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Счёт продолжается от введённого числа, а не от прежнего
    expected = 1.0 + PULSES * BASE_FACTOR / 1000
    assert abs(float(session.payload['ch1']) - expected) < 0.001, (
        f"ждали {expected}, приехало {session.payload['ch1']}")
    session.assert_delta(channel=1, liters=PULSES * BASE_FACTOR)


@pytest.mark.needs(ctype1=ELECTRONIC)
def test_D5_electronic_input_catches_short_pulses(stand: Stand) -> None:
    """
    Электронный вход считает импульс длиной в миллисекунду.

    Уровень снимается в прерывании по фронту и защёлкивается
    (`Attiny85/src/electronic.h`): главный цикл attiny за миллисекунду до входа
    не доходит - он спит, пишет EEPROM и общается с ЕСП. Само правило
    проверяют хостовые тесты, здесь - живой пин, настоящее прерывание и
    настоящая длительность.

    Тип «Электронный (+)» так не проверить: у стенда сухой контакт, а не
    источник уровня (04_not-tested.md).
    """
    stand.reset_observers()

    stand.dut.pulse(channel=1, count=PULSES, width_ms=SHORT_PULSE_MS)
    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Ровно столько, сколько подали: ни потерянных, ни удвоенных дребезгом
    session.assert_delta(channel=1, liters=PULSES * BASE_FACTOR)


def test_D6_electronic_high_input_counts_rising_pulses(stand: Stand) -> None:
    """
    Тип «Электронный (+)»: импульс - подъём линии, а не замыкание.

    Подтяжку attiny в этом типе выключает (`counter.h`, set_type), уровень
    задаёт счётчик - и в опыте его задаёт стенд: в покое линия прижата к
    земле, импульс - короткий подъём. Отпущенную линию высокоомный вход ловил
    бы как эфир.

    Идёт только там, где вход заведён на плату так, что ей можно выдавать
    уровень: без последовательного резистора плата упирает 3,3 В в вход,
    питающийся от батареек.
    """
    if not stand.cfg.can_drive_high:
        pytest.skip('стенд не выдаёт уровень на вход: [metf] can_drive_high в '
                    'stand.ini, обвязка - 04_not-tested.md')

    # Тип - в теле, а не маркером: без обвязки тест пропускается выше, и
    # перенастройка до него была бы впустую
    stand.setup(channel=1, ctype=ELECTRONIC_HIGH, factor=BASE_FACTOR)
    with stand.dut.driven(channel=1):
        stand.reset_observers()
        stand.dut.pulse_high(channel=1, count=PULSES, width_ms=HIGH_PULSE_MS)
        stand.dut.press_button()
        session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=PULSES * BASE_FACTOR)


# Серия поверх сеанса: импульс раз в две секунды двадцать секунд, нажатие на
# четвёртой - сеанс по кнопке длится столько же, и серия его накрывает
DURING_PULSES = 10
DURING_GAP_S = 2.0
PRESS_AFTER_S = 4.0


def test_D7_pulses_during_session_are_not_lost(stand: Stand) -> None:
    """
    Импульсы посреди сеанса не теряются и не удваиваются (#121, #238).

    #121: сто замыканий при отправке посреди серии давали 96-97. attiny
    считает и пока ЕСП на связи, а ЕСП берёт снимок в начале сеанса - поздние
    импульсы обязаны приехать следующим. Утверждается сумма двух приростов.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    train = threading.Thread(target=stand.dut.pulses, kwargs=dict(
        channel=1, count=DURING_PULSES, gap=DURING_GAP_S))
    train.start()
    try:
        time.sleep(PRESS_AFTER_S)
        stand.dut.press_button()
        first = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    finally:
        train.join()

    stand.reset_observers()
    stand.dut.press_button()
    second = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    assert first.payload is not None and second.payload is not None
    got = first.payload['delta1'] + second.payload['delta1']
    assert got == DURING_PULSES * BASE_FACTOR, (
        f"приросты {first.payload['delta1']} + {second.payload['delta1']}, "
        f'подано {DURING_PULSES} импульсов по {BASE_FACTOR} л')


PRESSES_WHILE_CLOSED = 4


def test_D8_input_held_closed_counts_once(stand: Stand) -> None:
    """
    Замкнутый вход - одно замыкание, сколько ни нажимай кнопку (#337, #76).

    Конец импульса attiny видит только по трём разомкнутым опросам подряд
    (`Attiny85/src/counter.h`, discrete). #337 открыт: геркон встал
    замкнутым, и каждое нажатие добавляло десять литров. Там причина может
    быть электрической - NAMUR у порога АЦП, - и сухим контактом стенд её не
    воспроизведёт; здесь закреплена сторона прошивки.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()
    stand.dut.press_button()
    start = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE).payload['imp1']

    seen = []
    try:
        stand.dut.wet(channel=1, closed=True)
        time.sleep(1.0)               # замыкание успевает попасть в опрос до нажатия
        for _ in range(PRESSES_WHILE_CLOSED):
            stand.reset_observers()
            stand.dut.press_button()
            seen.append(stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
                        .payload['imp1'])
    finally:
        stand.dut.wet(channel=1, closed=False)

    assert seen == [start + 1] * PRESSES_WHILE_CLOSED, (
        f'до замыкания {start}, по нажатиям {seen}: ждали ровно одно замыкание')


GLITCH_MS = 20           # короче подтверждения: IMPULSE_CONFIRM_MS = 50 (counter.h)
GLITCHES = 5
MERGE_GAP_S = 0.3        # короче трёх пустых опросов (750 мс): импульс не кончился


def test_D9_glitches_and_bounce_are_not_counted(stand: Stand) -> None:
    """
    Дребезг не даёт лишних импульсов (#371, #150, #200).

    Два правила attiny (`Attiny85/src/counter.h`, discrete): замыкание короче
    50 мс не переживает повторного чтения, а новое не считается, пока не
    прошло три пустых опроса. Первое проверяют короткие всплески, второе - два
    замыкания с паузой в треть секунды: это один импульс с дребезгом.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=GLITCHES, width_ms=GLITCH_MS)
    stand.dut.press_button()
    stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE).assert_delta(
        channel=1, liters=0)

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=2, gap=MERGE_GAP_S)
    stand.dut.press_button()
    stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE).assert_delta(
        channel=1, liters=BASE_FACTOR)


# Короче подтверждения discrete() (IMPULSE_CONFIRM_MS = 50): механическому
# входу это дребезг, электронному - обычный импульс
NAMUR_SHORT_MS = 50

# Длинное замыкание: опрос раз в 250 мс и переспрос через 50 мс укладываются
# внутрь с запасом. Триста миллисекунд легли бы на саму границу - переспрос
# пришёлся бы уже на отпущенный вход, и тест мигал бы на исправной прошивке
NAMUR_LONG_MS = 500

# «Не посчитан» обязано значить «не посчитан», а не «ещё не успел»: ждём
# заведомо дольше опроса, переспроса и трёх пустых опросов подряд
SETTLE_S = 3.0


def input_impulses(board: Any) -> int:
    """Сколько импульсов вход насчитал с начала сеанса настройки."""
    body = portal_mod.get_json(board, f'/api/status/{COLD}')
    assert 'error' not in body, f'нет связи с attiny: {body}'
    return int(body['impulses'])


def choose_type(board: Any, ctype: int) -> None:
    """Выбрать тип входа так, как это делает человек в портале."""
    answer = portal_mod.post_json(board, '/api/save_input_type',
                                  input=COLD, ctype=ctype)
    assert not answer.get('errors'), answer


@pytest.mark.portal
@pytest.mark.requires(attiny=43)
def test_D10_input_type_applies_without_leaving_portal(stand: Stand) -> None:
    """
    Выбранный в портале тип входа начинает действовать сразу, не дожидаясь
    следующего пробуждения.

    Так это и выглядит у человека при первичной настройке: он выбирает
    «Электронный», остаётся на странице определения счётчика и подаёт импульсы.
    До attiny 43 тип доезжал до счётчика только со следующим витком главного
    цикла, а до тех пор вход считался по-старому: короткие импульсы газового
    счётчика не ловились весь сеанс настройки - до десяти минут.

    Проверяется не ответ API, а факт счёта. Заявленный тип портал берёт у
    attiny, и та отдаёт его сразу (`set_counter_types`), так что на вид всё
    применилось бы и на сломанной прошивке.
    """
    if not stand.cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')

    with portal_mod.session(stand.cfg, stand) as board:
        choose_type(board, NAMUR)

        before = input_impulses(board)
        stand.dut.pulse(channel=COLD, count=1, width_ms=NAMUR_SHORT_MS)
        time.sleep(SETTLE_S)
        assert input_impulses(board) == before, (
            f'механический вход посчитал замыкание {NAMUR_SHORT_MS} мс')

        stand.dut.pulse(channel=COLD, count=1, width_ms=NAMUR_LONG_MS)
        time.sleep(SETTLE_S)
        assert input_impulses(board) == before + 1, (
            'механический вход не посчитал длинное замыкание')

        choose_type(board, ELECTRONIC)

        before = input_impulses(board)
        stand.dut.pulse(channel=COLD, count=PULSES, width_ms=SHORT_PULSE_MS)
        time.sleep(SETTLE_S)
        assert input_impulses(board) == before + PULSES, (
            f'электронный вход не посчитал импульсы {SHORT_PULSE_MS} мс: тип '
            'не дошёл до счётчика, пока идёт сеанс настройки')

        choose_type(board, NAMUR)

        before = input_impulses(board)
        stand.dut.pulse(channel=COLD, count=1, width_ms=NAMUR_SHORT_MS)
        time.sleep(SETTLE_S)
        assert input_impulses(board) == before, (
            f'вернули механический, а замыкание {NAMUR_SHORT_MS} мс всё ещё '
            'считается импульсом: прежний тип остался в счётчике')

        stand.dut.pulse(channel=COLD, count=1, width_ms=NAMUR_LONG_MS)
        time.sleep(SETTLE_S)
        assert input_impulses(board) == before + 1, (
            'после возврата к механическому длинное замыкание не считается')
