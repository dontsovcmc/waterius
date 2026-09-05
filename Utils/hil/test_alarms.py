"""
Тревоги на живом железе - блок E ручного плана.

Предусловие всей группы: attiny 41 и строка `Alarm config:` в логе. Пороги
живут в оперативной памяти attiny и уезжают туда командой в конце сеанса, так
что тест, не проверивший эту строку, зеленеет на выключенных тревогах. За
проверкой следит stand.setup_alarms().

Числа взяты с запасом от границ. Порог 3600 л/ч при весе 10 - это 40 тиков по
250 мс, то есть 10 секунд, а тревога поднимается при зазоре не больше 39 тиков.
Подавать импульсы с зазором 9,8 с - значит получить мигающий тест; берём 3 с.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .logwatch import ALARM_MODE, TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

# Типы входа, core/types.h
NAMUR = 0
LEAKAGE = 5
LEAKAGE_NC = 6

FACTOR = 10          # л/имп
FLOW_THRESHOLD = 3600  # л/ч -> 40 тиков -> 10 с между импульсами
LEAK_MINUTES = 2


def raise_flow_alarm(stand: Stand) -> None:
    """Поднять тревогу по расходу: два импульса с заведомым запасом от порога."""
    stand.dut.pulses(channel=1, count=2, gap=3.0)


def test_E1_flow_alarm(stand: Stand, quiet: None) -> None:
    """Расход выше порога поднимает тревогу и будит устройство вне расписания."""
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=FLOW_THRESHOLD,
                       ctype=NAMUR, vacation=0)
    stand.reset_observers()

    raise_flow_alarm(stand)

    session = stand.wait_session(timeout=120, mode=ALARM_MODE)
    session.assert_alarm(flow1=1, flow0=0)
    assert session.payload['alarm'] == 1
    assert session.payload['af1'] == FLOW_THRESHOLD


@pytest.mark.slow
def test_E2_flow_alarm_clears(stand: Stand, quiet: None, slow_clock: None) -> None:
    """
    Снятие тревоги - такая же новость, как её появление.

    Ждать приходится долго по двум причинам: расход считается упавшим при паузе
    вдвое длиннее порога (20,5 с), а внеплановые сеансы разделены паузой в пять
    минут, которая отсчитывается от конца предыдущего.
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=FLOW_THRESHOLD,
                       ctype=NAMUR, vacation=0)
    stand.reset_observers()

    raise_flow_alarm(stand)
    first = stand.wait_session(timeout=120, mode=ALARM_MODE)
    first.assert_alarm(flow1=1)
    assert first.confirm and first.confirm['confirmed'] == 1, (
        'пока доклад не подтверждён, состояние заморожено и снятия не будет')

    second = stand.wait_session(timeout=600, mode=ALARM_MODE)
    second.assert_alarm(flow1=0)


@pytest.mark.slow
@pytest.mark.xfail(reason='ложная протечка от одного импульса после долгой тишины')
def test_E3a_single_pulse_is_not_a_leak(stand: Stand, quiet: None,
                                        slow_clock: None) -> None:
    """
    Негативный контроль для непрерывного расхода.

    Один импульс и тишина - это не протечка. Тест существует потому, что без
    него проверка ритма зеленеет на сломанной логике: прошивка поднимает
    ALARM_LEAK и при полном отсутствии расхода, если перед импульсом была
    достаточно длинная пауза (Attiny85/src/alarm.h, prev_gap).
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_leak=LEAK_MINUTES,
                       ctype=NAMUR, vacation=0)
    stand.reset_observers()

    stand.dut.pulse(channel=1, count=1)

    session = stand.wait_session(timeout=(LEAK_MINUTES + 2) * 60)
    session.assert_alarm(leak1=0)


@pytest.mark.slow
def test_E3b_rhythm_raises_leak(stand: Stand, quiet: None, slow_clock: None) -> None:
    """Ровный расход дольше порога - протечка."""
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_leak=LEAK_MINUTES,
                       ctype=NAMUR, vacation=0)
    stand.reset_observers()

    stand.dut.pulses_every(channel=1, interval=25.0, minutes=LEAK_MINUTES + 1)

    session = stand.wait_session(timeout=180, mode=ALARM_MODE)
    session.assert_alarm(leak1=1)
    assert session.payload['al1'] == LEAK_MINUTES


@pytest.mark.slow
def test_E3c_household_profile_is_quiet(stand: Stand, quiet: None,
                                        slow_clock: None) -> None:
    """
    Обычный быт не должен выглядеть протечкой: расход, долгая пауза, снова
    расход. Пауза длиннее двойного интервала обязана сбросить счёт.
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_leak=LEAK_MINUTES,
                       ctype=NAMUR, vacation=0)
    stand.reset_observers()

    stand.dut.pulses(channel=1, count=3, gap=30.0)
    stand.expect_no_session(timeout=10 * 60)
    stand.dut.pulses(channel=1, count=3, gap=30.0)

    session = stand.wait_session(timeout=300)
    session.assert_alarm(leak1=0)


def test_E4_leak_sensor_closes(stand: Stand, quiet: None) -> None:
    """
    Датчик протечки: замыкание поднимает тревогу почти мгновенно.

    Единственная тревога с реакцией в пределах секунды - остальные ждут
    следующего импульса или пробуждения.
    """
    stand.setup(channel=0, ctype=LEAKAGE)
    stand.reset_observers()

    stand.dut.wet(channel=0, closed=True)
    try:
        session = stand.wait_session(timeout=90, mode=ALARM_MODE)
        session.assert_alarm(wet0=1)
    finally:
        stand.dut.wet(channel=0, closed=False)

    release = stand.wait_session(timeout=420, mode=ALARM_MODE)
    release.assert_alarm(wet0=0)


def test_E5_normally_closed_sensor_detects_cut_wire(stand: Stand, quiet: None) -> None:
    """
    Нормально-замкнутый датчик: обрыв провода - это тревога.

    Ради этого он и нужен: у нормально-разомкнутого перекушенный провод выглядит
    как «всё в порядке», и владелец считает себя защищённым.
    """
    stand.dut.wet(channel=0, closed=True)        # спокойное состояние - замкнуто
    stand.setup(channel=0, ctype=LEAKAGE_NC)
    stand.reset_observers()

    stand.dut.wet(channel=0, closed=False)       # обрыв
    session = stand.wait_session(timeout=90, mode=ALARM_MODE)
    session.assert_alarm(wet0=1)

    stand.dut.wet(channel=0, closed=True)


@pytest.mark.slow
def test_E6_vacation_mode(stand: Stand, quiet: None, slow_clock: None) -> None:
    """
    Режим «Я уехал»: тревогой становится любой расход.

    Проверяем по тикам в логе, а не по порогу в посылке: порог пользователя не
    затирается, подменяется только значение, уехавшее в attiny.
    """
    stand.setup_alarms(channel=1, factor=FACTOR, alarm_flow=FLOW_THRESHOLD,
                       ctype=NAMUR, vacation=0)

    on = stand.setup(vacation=1)
    assert on.alarm_config['interval1'] == 65535, (
        'в режиме «Я уехал» порог подменяется максимумом')
    assert on.alarm_config['vacation'] == 1

    stand.reset_observers()
    stand.dut.pulses(channel=1, count=2, gap=60.0)

    session = stand.wait_session(timeout=180, mode=ALARM_MODE)
    session.assert_alarm(flow1=1)

    off = stand.setup(vacation=0)
    assert off.alarm_config['interval1'] == 40, 'пороги пользователя должны вернуться'
    assert off.alarm_config['vacation'] == 0


def test_E10_vacation_works_without_factor(stand: Stand, quiet: None) -> None:
    """
    Режиму «Я уехал» известный вес импульса не нужен: проверка типа входа стоит
    до проверки веса, поэтому порог подменяется максимумом даже при «Авто».
    """
    stand.setup(channel=1, ctype=NAMUR, factor=3)     # 3 - это «Авто»
    on = stand.setup(vacation=1)
    assert on.payload['f1'] == 3, 'тест бессмысленен, если вес всё-таки задан'
    assert on.alarm_config['interval1'] == 65535

    stand.reset_observers()
    stand.dut.pulses(channel=1, count=2, gap=60.0)

    session = stand.wait_session(timeout=180, mode=ALARM_MODE)
    session.assert_alarm(flow1=1)

    stand.setup(vacation=0)


@pytest.mark.slow
def test_E7_consumption_stopped(stand: Stand, slow_clock: None) -> None:
    """
    Остановка расхода: её считает ЕСП, а не attiny, поэтому разрешение равно
    периоду пробуждения и считать надо пробуждения, а не минуты.
    """
    stand.setup(channel=1, alarm_stop=1, ctype=NAMUR, factor=FACTOR,
                period_min=5, send_on_consumption=0)
    stand.reset_observers()

    idle_before = None
    for _ in range(14):
        session = stand.wait_session(timeout=15 * 60, mode=TRANSMIT_MODE)
        idle = session.idle
        assert idle is not None, f'нет строки Idle min\n{session.text}'
        idle_before = idle
        if idle['stop1']:
            break
        assert idle['min1'] < 60, 'тревога должна была сработать на 60 минутах простоя'
    else:
        pytest.fail(f'остановка расхода не сработала: {idle_before}')

    assert session.payload['alarm_stop1'] == 1
    assert session.payload['alarm_stop0'] == 0

    stand.dut.pulse(channel=1, count=1)
    stand.dut.press_button()
    after = stand.wait_session(timeout=120)
    after.assert_alarm(stop1=0)
    assert after.idle['min1'] == 0
