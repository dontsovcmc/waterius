"""
Тревоги на живом железе - блок E ручного плана.

Предусловие всей группы: attiny 41 и строка `Alarm config:` в логе. Пороги
живут в оперативной памяти attiny и уезжают туда командой в конце сеанса, так
что тест, не проверивший эту строку, зеленеет на выключенных тревогах. За
проверкой следит stand.setup_alarms().

Главное свойство модели: **ни одна тревога не гаснет сама** - ни по времени, ни
по прекращению расхода (`Attiny85/src/alarm.h`). Отсюда три следствия для
тестов:

- снимать тревогу за собой обязан сам тест или фикстура `quiet`, иначе она
  достанется следующему и тот не дождётся своего внепланового сеанса;
- «тревога снялась» больше не наблюдается как отдельная новость от отпущенного
  датчика: снимает только человек - кнопкой, маской с сервера или сменой типа
  входа;
- негативные проверки («не снялась сама», «повторных сеансов нет») стали
  содержательными и вынесены в отдельные тесты.

Много воды сразу (E1, E2) и протечка по расходу (E3) помечены `experimental`: правила
детекции ещё могут измениться, по умолчанию эти тесты не идут. Гонять их -
`pytest --experimental`. Снятие, остановка потребления и негативный контроль
протечки (E3n) проверяются всегда.
Датчик протечки живёт в `test_leak_sensor.py`, режим «Я уехал» - в
`test_vacation.py`.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from .constants import (ALARM_WAIT_S, BASE_FACTOR, LEAKAGE, NAMUR, PLANNED_PERIOD_MIN,
                        PLANNED_WAIT_S, RESET_WET1, VOL_LITRES, VOL_PULSES)
from .logwatch import ALARM_MODE, MANUAL_TRANSMIT_MODE, TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

# Тревог до attiny 41 не существует: alarm_bits всегда 0, а send_alarm_config
# выходит на первой строке - вся группа бессмысленна.
pytestmark = [pytest.mark.stand, pytest.mark.requires(attiny=41)]


# Протечка: «расход не падал ниже 40 л/ч в течение часа». При весе 10 это квант
# тишины 14400*10/40 = 3600 тиков (15 минут) и 40/10 = 4 кванта подряд. Час
# здесь не выбран, а вычислен: квант * кванты всегда равны заданным часам, и
# быстрее протечку не поймать никаким подбором порогов.
RATE = 40              # л/ч
HOURS = 1
LEAK_QUANTUM_TICKS = 3600
LEAK_QUANTA = 4

# Сколько наблюдаем тишину после одного импульса: заданные часы и ещё два
# кванта по пятнадцать минут - ошибка счёта квантов успела бы сработать
LEAK_SILENCE_S = (HOURS * 60 + 2 * 15) * 60


def raise_volume_alarm(stand: Stand) -> None:
    """Поднять тревогу по объёму: порог набирается пятью импульсами."""
    stand.dut.pulses(channel=1, count=VOL_PULSES, gap=3.0)


def raise_both_channels(stand: Stand) -> None:
    """
    Поднять тревогу на каждом входе - по одной за раз.

    Порознь, а не разом: каждая тревога сама будит устройство, и поднятые
    вперемешку они дали бы сеанс с одной из двух, а какой именно - решал бы
    случай.

    Ждём любой сеанс, а не только тревожный: вторую тревогу attiny будит не
    раньше чем через ALARM_HOLD_MIN, и плановый сеанс, пришедший раньше, увозит
    её сам - тревожного после него уже не будет (Attiny85/src/main.cpp, «новость
    считается отданной независимо от режима»). Доставку отдельным сеансом
    проверяют тесты датчика, здесь нужны только две поднятые тревоги.

    Источник - датчики протечки на обоих входах: сценариям снятия нужны две
    независимые тревоги, а не конкретный их вид, и датчик единственный
    поднимается мгновенно и не зависит от веса импульса.

    Вход отпускается сразу: тревога от этого не гаснет, а замкнутый поднимал бы
    её заново на каждом тике, и снять её не смогли бы ни кнопка, ни маска.
    """
    for channel in (0, 1):
        stand.reset_observers()
        try:
            stand.dut.wet(channel=channel, closed=True)
            session = stand.wait_session(timeout=ALARM_WAIT_S)
            session.assert_alarm(**{f'wet{channel}': 1})
        finally:
            stand.dut.wet(channel=channel, closed=False)


@pytest.mark.experimental
def test_E1_volume_alarm(stand: Stand, quiet: None) -> None:
    """Объём за полчаса выше порога поднимает тревогу и будит устройство."""
    armed = stand.setup_alarms(channel=1, factor=BASE_FACTOR, alarm_vol=VOL_LITRES,
                               ctype=NAMUR, vacation=0)
    assert armed.alarm_config['vol1'] == VOL_PULSES, (
        f'литры не пересчитались в импульсы: {armed.alarm_config}')
    stand.reset_observers()

    raise_volume_alarm(stand)

    session = stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
    session.assert_alarm(flow1=1, flow0=0)
    assert session.payload['alarm'] is True
    assert session.payload['av1'] == VOL_LITRES


@pytest.mark.slow
@pytest.mark.experimental
def test_E2_alarm_does_not_clear_itself(stand: Stand, quiet: None) -> None:
    """
    Негативный контроль модели: тревога не гаснет сама.

    Прошлая прошивка снимала её по паузе в расходе, и именно это поведение
    убрано. Наблюдаем плановыми сеансами, а не кнопкой: кнопка тревогу снимает,
    и тест проверял бы собственное нажатие.
    """
    stand.setup_alarms(channel=1, factor=BASE_FACTOR, alarm_vol=VOL_LITRES,
                       ctype=NAMUR, vacation=0, period_min=PLANNED_PERIOD_MIN)
    stand.reset_observers()

    raise_volume_alarm(stand)
    # Любой сеанс: при периоде 5 минут плановый может увезти тревогу раньше
    # тревожного, если attiny ещё держит паузу после прошлой тревоги
    stand.wait_session(timeout=ALARM_WAIT_S).assert_alarm(flow1=1)

    # Расход прекращён, а тревога обязана остаться. Двух плановых сеансов
    # достаточно: старое правило снимало её через двадцать секунд тишины.
    for _ in range(2):
        planned = stand.wait_session(timeout=15 * 60, mode=TRANSMIT_MODE)
        planned.assert_alarm(flow1=1)


@pytest.mark.slow
@pytest.mark.experimental
def test_E3_leak_after_an_hour_without_silence(stand: Stand, quiet: None) -> None:
    """
    Протечка: за заданные часы не встретилось ни одного кванта тишины.

    Идёт час с лишним и короче быть не может - см. RATE и HOURS. Импульс раз в
    минуту при кванте в пятнадцать минут заведомо занимает каждый квант, так
    что тест не балансирует на границе.
    """
    armed = stand.setup_alarms(channel=1, factor=BASE_FACTOR, alarm_rate=RATE,
                               alarm_hours=HOURS, ctype=NAMUR, vacation=0)
    assert armed.alarm_config['quantum1'] == LEAK_QUANTUM_TICKS
    assert armed.alarm_config['quanta1'] == LEAK_QUANTA
    stand.reset_observers()

    stand.dut.pulses_every(channel=1, interval=60.0, minutes=HOURS * 60 + 5)

    session = stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
    session.assert_alarm(leak1=1)
    assert session.payload['ar1'] == RATE
    assert session.payload['ah1'] == HOURS


@pytest.mark.slow
def test_E3n_single_pulse_is_not_a_leak(stand: Stand, quiet: None) -> None:
    """
    Негативный контроль протечки: один импульс - не протечка (#405).

    Протечке нужны LEAK_QUANTA занятых квантов подряд (`Attiny85/src/alarm.h`,
    on_tick), один импульс занимает один. Без метки experimental: ложная
    тревога от одного импульса недопустима при любых правилах детекции.

    Свидетельство - отсутствие тревожного сеанса: attiny будит ЕСП на каждую
    новую тревогу, а проверка кнопкой сняла бы тревогу до чтения.
    """
    armed = stand.setup_alarms(channel=1, factor=BASE_FACTOR, alarm_rate=RATE,
                               alarm_hours=HOURS, ctype=NAMUR, vacation=0)
    assert armed.alarm_config['quantum1'] == LEAK_QUANTUM_TICKS
    assert armed.alarm_config['quanta1'] == LEAK_QUANTA
    stand.reset_observers()

    stand.dut.pulse(channel=1, count=1)

    stand.expect_no_session(timeout=LEAK_SILENCE_S, mode=ALARM_MODE)


@pytest.mark.needs(ctype0=LEAKAGE, ctype1=LEAKAGE)
def test_E13_button_clears_both_channels(stand: Stand, quiet: None) -> None:
    """
    Короткое нажатие кнопки снимает всё разом, не дожидаясь квитанции.

    Нажал - значит увидел: человек стоит у прибора, и ждать конца сеанса в
    шкафу ему незачем. Поэтому снятие видно уже в том сеансе, который кнопка и
    подняла, а не в следующем, как при снятии маской.
    """
    raise_both_channels(stand)

    stand.reset_observers()
    stand.dut.press_button()
    cleared = stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    cleared.assert_alarm(wet0=0, wet1=0)


@pytest.mark.slow
@pytest.mark.needs(ctype0=LEAKAGE, ctype1=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
def test_E17_reset_mask_clears_only_its_bits(stand: Stand, quiet: None) -> None:
    """
    Маска с сервера снимает ровно то, что в ней.

    Две тревоги на разных каналах, маска - на одну. Тест ловит перепутанную
    раскладку: в кадре 'A' каналы лежат вплотную (биты 0-2 и 3-5), а в
    `Header.flags` - со сдвигом под флаг питания. На одном канале ошибка сдвига
    незаметна, на двух - нет.

    Маска уезжает плановым сеансом: кнопка сняла бы обе тревоги сама.
    """
    raise_both_channels(stand)

    applied = stand.setup(arst=RESET_WET1, wake=False, timeout=PLANNED_WAIT_S)
    assert applied.alarm_config['reset'] == RESET_WET1, (
        f'маска не уехала в attiny: {applied.alarm_config}')

    # В этом сеансе посылка собрана из снимка, снятого до снятия тревоги
    stand.reset_observers()
    cleared = stand.wait_session(timeout=PLANNED_WAIT_S)
    cleared.assert_alarm(wet1=0, wet0=1)


@pytest.mark.slow
def test_E7_consumption_stopped(stand: Stand) -> None:
    """
    Остановка расхода: её считает ЕСП, а не attiny, поэтому разрешение равно
    периоду пробуждения и считать надо пробуждения, а не минуты.

    Эта тревога живёт по своим правилам и снимается сама первым же импульсом:
    её состояние ЕСП пересчитывает в каждом сеансе, а не хранит в attiny.
    """
    stand.setup(channel=1, alarm_stop=1, ctype=NAMUR, factor=BASE_FACTOR,
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

    assert session.payload['alarm_stop1'] is True
    assert session.payload['alarm_stop0'] is False

    stand.dut.pulse(channel=1, count=1)
    stand.dut.press_button()
    after = stand.wait_session(timeout=120)
    after.assert_alarm(stop1=0)
    assert after.idle['min1'] == 0
