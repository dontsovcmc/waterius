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
from .constants import BASE_FACTOR, COLD, ELECTRO, ELECTRONIC, ELECTRONIC_HIGH, NAMUR
from .dut import IMPULSE_GAP_S, IMPULSE_WIDTH_MS, WAKE_MARGIN_MS, Series
from .logwatch import MANUAL_TRANSMIT_MODE

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand  # а сбор тестов должен работать без них

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


def test_D2a_missed_session_keeps_consumption(stand: Stand, net: Any) -> None:
    """
    Пропущенный сеанс не теряет расход.

    Точка отсчёта двигается в конце сеанса, а он выполняется только при
    поднятом Wi-Fi. Значит при выключенной точке доступа импульсы обоих
    периодов приедут одной посылкой.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    with net.ap_off():
        stand.dut.pulse(channel=1, count=PULSES)
        stand.dut.press_button()
        missed = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
        assert not missed.wifi_connected

    stand.dut.pulse(channel=1, count=PULSES)
    stand.dut.press_button()
    session = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=2 * PULSES * BASE_FACTOR)


@pytest.mark.requires(esp='2.0.47')       # младшие двигают точку отсчёта после коннекта
def test_D2b_undelivered_session_keeps_delta(stand: Stand, net: Any) -> None:
    """
    Тот же опыт, но сеть жива, а получатели недоступны.

    Прирост не теряется от того, что посылка не доехала: точка отсчёта
    двигается после доставки, а не после подключения к Wi-Fi.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    with net.internet_down():
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
    payload = before.expect_payload()
    kwh_before = float(payload[f'ch{channel}'])
    imp_before = int(payload[f'imp{channel}'])

    stand.reset_observers()
    подано = stand.dut.pulse(channel=channel, count=per_kwh)
    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Счётчик импульсов attiny приезжает в той же посылке (`json.cpp`, imp0), и
    # без него расхождение в киловатт-часах ничего не говорит: непонятно, то ли
    # прошивка посчитала не так, то ли импульс не дошёл до входа. Прогон 20.09
    # дал ровно такой немой отчёт - 100,9 вместо 101,0, и причина осталась
    # неизвестной
    got = float(session.payload[f'ch{channel}'])
    counted = int(session.payload[f'imp{channel}']) - imp_before
    # Сперва воздействие: плата отдаёт расписку о фронтах, которые правда выдала
    # (`/pulse/stat`), и без неё «дошло 9 из 10» не отличить от «стенд не довёл»
    assert подано == per_kwh, (
        f'плата выдала {подано} замыканий из {per_kwh} по своей расписке: '
        f'воздействия не было, и об учёте прошивки этот тест ничего не говорит')
    assert counted == per_kwh, (
        f'плата выдала {per_kwh} замыканий, а attiny насчитала {counted}: '
        f'импульс потерян между выводом платы и входом устройства - шлейф, '
        f'уровень или сам вход. Показания: было {kwh_before}, стало {got}')
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

    ch1 = lowered.expect_payload()['ch1']
    assert abs(float(ch1) - 1.0) < 0.001, f'показания не сбросились: {ch1}'

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

    Источника уровня этому типу не нужно: подтяжку attiny держит включённой,
    покой - высокий уровень, и стенду остаётся замкнуть на минус. Тип
    «Электронный (+)» устроен наоборот, и его проверяет test_D6.
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

    Уровень подъёма - 3,3 В платы, и на пятивольтовом attiny этого хватает:
    порог высокого уровня 0,6·VCC = 3,0 В. Величина не теоретическая - в пачке
    замыканий уровни чередуются, поэтому паузы плата держит теми же 3,3 В, и
    зелёный test_D5 означает, что attiny берёт их за покой.

    Тип - в теле, а не маркером: маркер выставил бы его фикстурой, до того как
    стенд возьмёт линию, а этого делать нельзя. Смена типа гасит подтяжку, и
    отпущенный высокоомный вход с включённым прерыванием ловит эфир - лишние
    импульсы легли бы в проверяемую дельту и выглядели бы виной прошивки.
    Поэтому сначала минус на линии, и только потом тип.
    """
    with stand.dut.driven(channel=1):
        stand.setup(channel=1, ctype=ELECTRONIC_HIGH, factor=BASE_FACTOR)
        stand.reset_observers()
        подано = stand.dut.pulse_high(channel=1, count=PULSES,
                                      width_ms=HIGH_PULSE_MS)
        # Сначала приговор стенду: расхождение заказа и расписки платы - его
        # беда, и отказ обязан это говорить, иначе недоданный импульс выглядит
        # как недосчитавшая прошивка
        assert подано == PULSES, (
            f'стенд заказал {PULSES} импульсов подъёма по {HIGH_PULSE_MS} мс, '
            f'а плата по своей расписке выдала {подано}: это про стенд, а не '
            f'про прошивку\n{stand.dut.delivered(channel=1)}')
        stand.dut.press_button()
        session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    session.assert_delta(channel=1, liters=PULSES * BASE_FACTOR)


# Серия поверх сеанса: импульс раз в две секунды двадцать секунд, нажатие на
# четвёртой - сеанс по кнопке длится столько же, и серия его накрывает
DURING_PULSES = 10
DURING_GAP_S = 2.0
PRESS_AFTER_S = 4.0


def counted(session: Any, channel: int = 1) -> str:
    """Строки attiny о счёте входа - чем платить за разбор потери."""
    mark = f'impulses{channel}'
    return ' | '.join(line.split(': ', 1)[-1] for line in session.text.splitlines()
                      if mark in line) or f'в логе нет строк {mark}'


def test_D7_pulses_during_session_are_not_lost(stand: Stand) -> None:
    """
    Импульсы посреди сеанса не теряются и не удваиваются (#121, #238).

    #121: сто замыканий при отправке посреди серии давали 96-97. attiny
    считает и пока ЕСП на связи, а ЕСП берёт снимок в начале сеанса - поздние
    импульсы обязаны приехать следующим. Утверждается сумма двух приростов.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)
    stand.reset_observers()

    # Поток молча съедает исключение, а join о нём не скажет: серия, сорвавшаяся
    # на осечке связи с платой, выглядела бы как потерянные приросты
    итог: list[int] = []
    беда: list[BaseException] = []

    def поезд() -> None:
        try:
            итог.append(stand.dut.pulses(
                channel=1, count=DURING_PULSES, gap=DURING_GAP_S))
        except BaseException as err:            # noqa: BLE001
            беда.append(err)

    train = threading.Thread(target=поезд)
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

    got = (first.expect_payload('первый сеанс')['delta1']
           + second.expect_payload('второй сеанс')['delta1'])
    # Счётчик attiny в отказ: он делит вину. Насчитала attiny все импульсы -
    # прирост потеряла ЕСП; насчитала меньше - импульс не доехал до платы, и
    # виноват стенд или дребезг, а не прошивка
    if беда:
        raise AssertionError(
            f'стенд не довёл серию импульсов: {беда[0]!r}') from беда[0]
    assert итог == [DURING_PULSES], (
        f'плата приняла {итог} замыканий из {DURING_PULSES}: воздействия не было, '
        f'и о потере приростов этот тест не говорит')
    assert got == DURING_PULSES * BASE_FACTOR, (
        f"приросты {first.payload['delta1']} + {second.payload['delta1']}, "
        f'плата выдала {DURING_PULSES} замыканий по {BASE_FACTOR} л\n'
        f'счёт attiny: {counted(first)} -> {counted(second)}')


# Серия поверх пробуждения: начинается до нажатия, кончается после сеанса.
# Сеанс по кнопке на стенде - около 5 с (замер 06.10)
OVER_PRESS_AFTER = 2

# Механический вход: замыкание 350 мс ловит опрос раз в 250 мс с переспросом
# через 50 мс, конец - три пустых опроса (counter.h, discrete): секунда паузы
# вмещает их и при такте watchdog 275 мс
MECH_PULSES = 10
MECH_PULSE_MS = 350
MECH_GAP_MS = 1000

# Электронный вход: после засчитанного импульса мёртвое время до следующего
# такта 250 мс (electronic.h, take), шаг 300 мс - сразу за ним
ELECTRONIC_PULSE_MS = 10
ELECTRONIC_STEP_MS = 300
ELECTRONIC_PULSES = 10_000 // ELECTRONIC_STEP_MS

# Меньше - серия не накрыла сеанс, и о счёте посреди него опыт не говорит.
# Механика при шаге 1,35 с кладёт в пятисекундный сеанс три, запас на быстрый
MIN_INSIDE = 2


def over_session(stand: Stand, shapes: dict[int, tuple[int, int]],
                 **series_args: int) -> tuple[Series, Any, Any]:
    """Серия с нажатием посреди неё, её сеанс и следующий по кнопке."""
    stand.reset_observers()
    series = stand.dut.series(shapes, press_after=OVER_PRESS_AFTER, **series_args)
    first = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)

    stand.reset_observers()
    stand.dut.press_button()
    second = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    return series, first, second


def assert_counted_over_session(series: Series, first: Any, second: Any,
                                channel: int, count: int) -> None:
    """
    Импульсы входа поданы целиком, часть легла в сеанс ЕСП, а сумма двух
    приростов равна поданному.
    """
    closures = series.closures[channel]
    assert len(closures) == count, (
        f'на входе {channel} плата по расписке выдала {len(closures)} импульсов '
        f'из {count}: это про стенд, а не про прошивку')

    length = first.length_ms
    assert length is not None, (
        f'у последней строки сеанса нет метки времени ЕСП\n{first.text}')
    inside = series.awake(channel, length)
    assert len(inside) >= MIN_INSIDE, (
        f'в сеанс ЕСП ({length} мс, окно с {WAKE_MARGIN_MS} мс после нажатия) '
        f'легло {len(inside)} импульсов входа {channel}, нужно {MIN_INSIDE}: '
        f'серия не накрыла сеанс, и это про стенд. Нажатие {series.press}, '
        f'импульсы {closures}')

    got = (first.expect_payload('первый сеанс')[f'delta{channel}']
           + second.expect_payload('второй сеанс')[f'delta{channel}'])
    assert got == count * BASE_FACTOR, (
        f"delta{channel}: {first.payload[f'delta{channel}']} + "
        f"{second.payload[f'delta{channel}']}, а подано {count} импульсов "
        f'по {BASE_FACTOR} л, из них {len(inside)} посреди сеанса\n'
        f'счёт attiny: {counted(first, channel)} -> {counted(second, channel)}')


@pytest.mark.needs(ctype1=ELECTRONIC)
def test_D7b_electronic_pulses_during_session_are_not_lost(stand: Stand) -> None:
    """
    Импульсы электронного входа посреди сеанса не теряются и не удваиваются.

    В сеансе главный цикл attiny занят ЕСП: отвечает по i2c и пишет EEPROM, и
    импульс держит только защёлка в прерывании (`Attiny85/src/electronic.h`).
    D5 проверяет её во сне, здесь - десять секунд импульсов так часто, как
    вход их принимает, пока ЕСП включена.
    """
    series, first, second = over_session(
        stand, {1: (ELECTRONIC_PULSE_MS, 0)}, count=ELECTRONIC_PULSES,
        step_ms=ELECTRONIC_STEP_MS,
        min_gap_ms=ELECTRONIC_STEP_MS - ELECTRONIC_PULSE_MS)

    assert_counted_over_session(series, first, second, channel=1,
                                count=ELECTRONIC_PULSES)


@pytest.mark.needs(ctype0=NAMUR, ctype1=ELECTRONIC, f0=BASE_FACTOR, f1=BASE_FACTOR)
def test_D7c_two_input_types_count_during_session(stand: Stand) -> None:
    """
    Механический и электронный входы посреди сеанса считают каждый своё.

    Форма та же, что в D11: импульс электронного входа ложится внутрь каждого
    замыкания механического. D11 проверяет это во сне, здесь - пока attiny
    ещё и обслуживает ЕСП.
    """
    series, first, second = over_session(
        stand, {0: (MECH_PULSE_MS, 0), 1: (ELECTRONIC_PULSE_MS, INSIDE_MS)},
        count=MECH_PULSES, step_ms=MECH_PULSE_MS + MECH_GAP_MS,
        min_gap_ms=MECH_GAP_MS)

    assert_counted_over_session(series, first, second, channel=0, count=MECH_PULSES)
    assert_counted_over_session(series, first, second, channel=1, count=MECH_PULSES)


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

# Потолок на поданную паузу: короче двух тактов опроса в неё не влезает третий
# разомкнутый опрос ни при какой фазе, то есть слипание гарантировано. На
# 500-749 мс третий опрос уже может попасть, и два импульса будут законны -
# такой паузой проверять слипание нельзя
MERGE_CEILING_MS = 500


def test_D9_glitches_and_bounce_are_not_counted(stand: Stand) -> None:
    """
    Дребезг не даёт лишних импульсов (#371, #150, #200).

    Два правила attiny (`Attiny85/src/counter.h`, discrete): замыкание короче
    50 мс не переживает повторного чтения, а новое не считается, пока не
    прошло три пустых опроса. Первое проверяют короткие всплески, второе - два
    замыкания с паузой в треть секунды: это один импульс с дребезгом.

    Воздействие уходит одной пачкой, и интервалы в ней отмеряет плата. Пока
    паузу набирал стенд, к ней приклеивалась дорога запроса - заказанные 0,3 с
    приходили как 2 с, attiny справедливо считала два импульса, а падал тест
    прошивки. Поэтому перед приговором стенд спрашивает плату, что подал.
    """
    stand.setup(channel=1, factor=BASE_FACTOR, ctype=NAMUR, period_min=120)

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=GLITCHES, width_ms=GLITCH_MS)
    stand.dut.press_button()
    stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE).assert_delta(
        channel=1, liters=0)

    stand.reset_observers()
    stand.dut.pulse(channel=1, count=2, gap=MERGE_GAP_S)

    # Расписку читаем до нажатия: кнопка - такой же импульс, и она её заменит
    delivered_ms = stand.dut.delivered(channel=1)
    assert delivered_ms[1] < MERGE_CEILING_MS, (
        f'стенд подал паузу {delivered_ms[1]} мс, а слипание проверяется паузой '
        f'короче {MERGE_CEILING_MS} мс - по такой attiny обязана посчитать два '
        f'импульса. Это отказ стенда, не прошивки (участки {delivered_ms})')

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


# Механический вход: замыкания с паузой IMPULSE_GAP_S - пауза больше 750 мс,
# поэтому каждое замыкание отдельный импульс. Электронный: импульс в
# миллисекунду внутрь каждого замыкания, со смещением от его начала
TOGETHER = 3
INSIDE_MS = 100


@pytest.mark.needs(ctype0=NAMUR, ctype1=ELECTRONIC, f0=BASE_FACTOR, f1=BASE_FACTOR)
def test_D11_two_input_types_count_together(stand: Stand) -> None:
    """
    Входы разных типов считают каждый своё и не мешают друг другу.

    Типы обслуживает разный код (`Attiny85/src/counter.h`, is_impuls), но живут
    они в одном цикле и на одном прерывании: `ISR(PCINT0_vect)` зовёт `on_front`
    у обоих счётчиков (`Attiny85/src/main.cpp`), а механический вход посреди
    опроса встаёт на 50 мс переспроса и включает АЦП. Импульс электронного входа
    длиной в миллисекунду обязан это пережить, а механический - не набрать
    лишнего от чужих фронтов.

    Одновременность здесь заказана, а не подгадана: обе линии уезжают одной
    пачкой, и плата выставляет их от одного старта.
    """
    closed_ms = IMPULSE_WIDTH_MS
    gap_ms = int(IMPULSE_GAP_S * 1000)
    step_ms = closed_ms + gap_ms          # шаг замыканий механического входа

    mech: list[int] = []
    for i in range(TOGETHER):
        if i:
            mech.append(gap_ms)
        mech.append(closed_ms)

    electro = [1]
    for _ in range(TOGETHER - 1):
        electro += [step_ms - 1, 1]

    stand.reset_observers()
    stand.dut.wave(stand.dut.line(channel=0, edges=mech),
                   stand.dut.line(channel=1, edges=electro, at_ms=INSIDE_MS))

    # Опыт состоялся, только если каждый короткий импульс лёг внутрь замыкания
    marks0 = stand.dut.moments(channel=0)
    marks1 = stand.dut.moments(channel=1)
    closures = list(zip(marks0[0::2], marks0[1::2], strict=False))
    for i, start in enumerate(marks1[0::2]):
        assert any(low <= start <= high for low, high in closures), (
            f'импульс {i + 1} электронного входа ушёл в {start}, а механический '
            f'был замкнут {closures}: перекрытия не было. Это про стенд')

    stand.dut.press_button()
    session = stand.wait_session(timeout=120, mode=MANUAL_TRANSMIT_MODE)

    # Механический вход: одно замыкание - один импульс, сколько бы опросов оно
    # ни накрыло; электронный - по импульсу на каждый фронт
    session.assert_delta(channel=0, liters=TOGETHER * BASE_FACTOR)
    session.assert_delta(channel=1, liters=TOGETHER * BASE_FACTOR)
