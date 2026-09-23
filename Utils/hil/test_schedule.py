"""
Расписание и режим «только при расходе» - блок H ручного плана.

Все проверки здесь диапазонные. Время отмеряет сторожевой таймер attiny,
частота которого гуляет от экземпляра к экземпляру и от температуры, а ЕСП
компенсирует это поправкой. Требовать точных минут - значит завести мигающий
тест на исправной прошивке.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING, Any

import pytest

from . import portal as portal_mod
from .constants import NAMUR
from .logwatch import MANUAL_TRANSMIT_MODE, TRANSMIT_MODE

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand  # а сбор тестов должен работать без них

pytestmark = [pytest.mark.stand, pytest.mark.slow]

PERIOD_MIN = 5

# Допуск интервала: после смены периода attiny заказывают 90 % от него
# (period_after_user_change), к этому - длительность сеанса. Отличить 15 минут
# от 5 (регресс PR #396) он позволяет с запасом
H1_TOLERANCE = 0.4

# Регресс PR #396 будил устройство каждые пять минут при периоде пятнадцать, и
# на пятиминутном его не видно. Короткие периоды проверяют H6 (10) и H7 (5)
H1_PERIOD_MIN = 15


def test_H1_wakeup_period(stand: Stand) -> None:
    """
    Период пробуждения соблюдается: устройство само выходит на связь раз в
    заданные минуты.

    Измеряем по факту, а не по логу: время отмеряет сторожевой таймер attiny, и
    единственное честное доказательство - интервалы между сеансами.

    Режим «только при расходе» здесь не трогаем: выключенным его держат общие
    требования стенда, а на прошивках младше 2.0.47 такой настройки нет вовсе.
    """
    period = H1_PERIOD_MIN
    stand.setup(period_min=period)

    stand.reset_observers()
    stamps = []
    for _ in range(3):
        session = stand.wait_session(timeout=2 * period * 60 + 120, mode=TRANSMIT_MODE)
        stamps.append(time.time())
        assert session.mode == TRANSMIT_MODE

    low, high = period * (1 - H1_TOLERANCE), period * (1 + H1_TOLERANCE)
    for before, after in zip(stamps, stamps[1:], strict=False):
        minutes = (after - before) / 60
        assert low <= minutes <= high, (
            f'интервал между сеансами {minutes:.1f} мин при периоде {period}')


@pytest.mark.requires(esp='2.0.47')       # младшие не печатают период attiny в лог
def test_H1b_period_reaches_attiny(stand: Stand) -> None:
    """
    Заказанный период доезжает до attiny.

    В логе будет 4, а не 5: при смене периода пользователем накопленная поправка
    перестаёт годиться, и прошивка заказывает заведомо меньшее значение -
    проснуться раньше цели не страшно, проснуться позже значит проехать точку.
    """
    session = stand.setup(period_min=PERIOD_MIN)

    assert session.period_attiny is not None, (
        f'в логе нет строки про период attiny\n{session.text}')
    assert 3 <= session.period_attiny <= PERIOD_MIN, (
        f'в attiny уехал период {session.period_attiny}')


@pytest.mark.requires(esp='2.0.47')       # у младших нет режима «только при расходе»
def test_H3_silent_when_no_consumption(stand: Stand) -> None:
    """
    Режим «только при расходе»: без воды устройство просыпается, но молчит.

    Пробуждение при этом есть, и в логе оно выглядит завершённым сеансом
    mode=2 - «сеанса не было» здесь неверно. Проверяется, что в этом сеансе
    не поднималось радио и не ушла посылка.

    Наблюдаем со второго пробуждения: первое после кнопки - разовая передача,
    она выходит на связь всегда.
    """
    stand.setup(period_min=PERIOD_MIN, send_on_consumption=1, channel=1, ctype=NAMUR)
    stand.reset_observers()

    session = stand.wait_session(timeout=3 * PERIOD_MIN * 60, mode=TRANSMIT_MODE)

    idle = session.idle_send
    assert idle is not None, f'нет строки Idle:\n{session.text}'
    assert idle['consumed'] == 0, f'расхода не было, а прошивка его насчитала: {idle}'
    assert idle['transmit'] == 0, f'прошивка решила выйти на связь: {idle}'
    assert 'Idle: no consumption, WiFi stays off' in session.text, session.text
    assert not session.wifi_connected, session.text
    assert session.payload is None, 'без расхода посылки быть не должно'


@pytest.mark.requires(esp='2.0.47')       # у младших нет режима «только при расходе»
def test_H4_consumption_wakes_it_up(stand: Stand) -> None:
    """Импульс возвращает связь на ближайшем плановом пробуждении."""
    stand.setup(period_min=PERIOD_MIN, send_on_consumption=1, channel=1, ctype=NAMUR)
    stand.reset_observers()

    stand.dut.pulse(channel=1, count=2)

    session = stand.wait_session(timeout=3 * PERIOD_MIN * 60, mode=TRANSMIT_MODE)
    idle = session.idle_send
    assert idle is not None, f'нет строки Idle:\n{session.text}'
    assert idle['consumed'] == 1
    assert idle['transmit'] == 1
    assert session.payload is not None and session.payload['delta1'] > 0


def planned_wait(period: int) -> float:
    """Сколько ждать планового сеанса: два периода и запас на сам сеанс."""
    return 2 * period * 60 + 120


def assert_interval(minutes: float, period: int, what: str) -> None:
    low, high = period * (1 - H1_TOLERANCE), period * (1 + H1_TOLERANCE)
    assert low <= minutes <= high, f'{what}: {minutes:.1f} мин при периоде {period}'


MANUAL_PERIOD_MIN = 10


@pytest.mark.requires(esp='2.0.47')
def test_H6_manual_wakeup_restarts_schedule(stand: Stand) -> None:
    """
    Ручное пробуждение задаёт расписание заново: следующий плановый сеанс -
    через целый период после нажатия, а не в прежней точке (#380).

    Нажатие посреди периода разводит одно и другое: прежняя точка была бы
    через полпериода, новая - через целый.
    """
    stand.setup(period_min=MANUAL_PERIOD_MIN)
    stand.reset_observers()
    stand.wait_session(timeout=planned_wait(MANUAL_PERIOD_MIN), mode=TRANSMIT_MODE)

    time.sleep(MANUAL_PERIOD_MIN * 60 / 2)
    stand.reset_observers()
    stand.dut.press_button()
    stand.wait_session(timeout=180, mode=MANUAL_TRANSMIT_MODE)
    pressed = time.time()

    stand.wait_session(timeout=planned_wait(MANUAL_PERIOD_MIN), mode=TRANSMIT_MODE)
    assert_interval((time.time() - pressed) / 60, MANUAL_PERIOD_MIN,
                    'плановый сеанс после нажатия')


@pytest.mark.portal
@pytest.mark.requires(attiny=40, esp='2.0.47')
def test_H7_esp_reset_keeps_period(stand: Stand, cfg: Any) -> None:
    """
    Перезагрузка ЕСП без ведома attiny: флаг перезагрузки поднят, период цел
    (#354, #350, #242).

    #242, #350: после перезагрузки ЕСП устройство переходило на период по
    умолчанию и выходило на связь каждые 15 минут вместо часа. Флаг - слово
    самой прошивки (`main.cpp`, строка `esp restarted:`), по нему портал
    показывает плашку 22. Обычный сеанс в начале теста - негативный контроль.

    Перезагружаемся выходом из режима настройки, а не линией сброса стенда:
    она заведена на вывод reset attiny и перезагружает всю плату, а тут нужна
    одна ЕСП. Выход из портала даёт ровно это: `/api/turnoff` ставит
    `exit_portal_flag`, и прошивка зовёт `ESP.restart()` (`main.cpp`), не
    сказав attiny «ухожу спать». Питание остаётся поданным, attiny держит
    взведённым ESP_POWERED_LONG, и по нему прошивка на следующей загрузке
    узнаёт себя перезагруженной - тот же признак, что и после падения по
    сторожевому таймеру.

    Сам путь описан в `docs/setup-portal.md`, раздел «Завершение и сброс»; то,
    что плашка о перезагрузке после него показывается штатно, - в
    `docs/known-gaps.md`.
    """
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')

    normal = stand.setup(period_min=PERIOD_MIN)
    assert 'esp restarted: 0' in normal.full_text, normal.full_text

    board = portal_mod.open_portal(cfg, stand)
    try:
        # Портал поднимался секунды, ESP_POWERED_LONG_MSEC (5 с) позади:
        # attiny уже не отличит перезагрузку от обычного включения только по
        # времени, а нам именно это и нужно
        stand.reset_observers()
        portal_mod.turnoff(board)
    finally:
        board.close()

    rebooted = stand.wait_session(timeout=180)
    assert 'esp restarted: 1' in rebooted.full_text, rebooted.full_text
    assert rebooted.complete, rebooted.text
    done = time.time()

    stand.wait_session(timeout=planned_wait(PERIOD_MIN), mode=TRANSMIT_MODE)
    assert_interval((time.time() - done) / 60, PERIOD_MIN,
                    'плановый сеанс после перезагрузки')


@pytest.mark.requires(esp='2.0.47')
def test_H8_missed_session_keeps_tuning(stand: Stand) -> None:
    """
    Пропущенный сеанс не ломает подстройку периода (#345, #347).

    #347: после пропуска поправка считалась по двойному сну как по одному
    периоду, и период уезжал вдвое. Правило проверяют хостовые test_wakeup,
    здесь - настоящий пропуск: плановый сеанс без сети.
    """
    stand.setup(period_min=PERIOD_MIN)
    stand.reset_observers()
    before = stand.wait_session(timeout=planned_wait(PERIOD_MIN), mode=TRANSMIT_MODE)
    assert before.payload is not None
    tuned = before.payload['period_min_tuned']

    with stand.net.ap_off():
        missed = stand.wait_session(timeout=planned_wait(PERIOD_MIN), mode=TRANSMIT_MODE)
        assert not missed.wifi_connected, missed.text

    after = stand.wait_session(timeout=planned_wait(PERIOD_MIN), mode=TRANSMIT_MODE)
    assert after.payload is not None, 'после пропуска посылки нет'
    now = after.payload['period_min_tuned']
    assert abs(now - tuned) <= 0.3 * tuned, f'поправка уехала после пропуска: {tuned} -> {now}'
