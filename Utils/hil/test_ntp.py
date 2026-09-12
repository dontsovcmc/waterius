"""
Синхронизация времени - блок N.

Время отдаёт плата стенда (METF, протокол 6), а не интернет и не машина с
прогоном. Поэтому все проверки здесь локальные: что бы ни происходило с
`ru.pool.ntp.org`, результат теста от этого не зависит.

Время назначает тест, и назначает заведомо узнаваемым - на час вперёд от
настоящего. Тогда «устройство взяло время у нас» отличается от «взяло откуда-то
ещё» одним взглядом на метку в посылке. Вперёд, а не назад: прошивка хранит
время последней отправки, и переведённые назад часы дают ей состояние
«проснулись раньше, чем заснули».

Чего здесь нет. Две синхронизации с суточным разрывом стенд не проверит - это
сутки прогона. Проверяется соседнее и достижимое: что после прогрева устройство
перестаёт спрашивать время каждое пробуждение.
"""

from __future__ import annotations

import re
import time
from datetime import datetime, timezone
from typing import TYPE_CHECKING, Any, Iterator

import pytest

from .logwatch import TRANSMIT_MODE
if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = [pytest.mark.stand, pytest.mark.requires(esp='2.0.47')]

# Умолчание прошивки: с этим значением пользовательский сервер не используется
# и время берётся из пула (`ESP8266/src/sync_time.cpp`, sync_ntp_time)
DEFAULT_NTP_SERVER = 'ru.pool.ntp.org'

NTP_POOL_SIZE = 4          # core/timekeeping.h
NTP_WARMUP_SYNCS = 2       # столько синхронизаций подряд, потом раз в сутки

SKEW_SEC = 3600            # на сколько часы стенда отличаются от настоящих
SKEW_TOLERANCE = 600       # допуск при сверке: сеанс идёт до двух минут

SHORT_PERIOD_MIN = 2       # период на время проверки троттлинга

# Имя сервера прошивка печатает и когда резолв удался, и когда нет: в обоих
# случаях видно, какой из пула она выбрала
NTP_NAME = re.compile(r'NTP: (?:NtpServer|Unable to resolve) (\d)\.ru\.pool\.ntp\.org')


def board_epoch() -> int:
    """Время, которое назначаем плате: узнаваемое, но не в прошлом."""
    return int(time.time()) + SKEW_SEC


def payload_epoch(payload: dict[str, Any]) -> int:
    """Метка времени посылки в секундах unix."""
    stamp = payload['timestamp']
    return int(datetime.strptime(stamp, '%Y-%m-%dT%H:%M:%S%z')
               .astimezone(timezone.utc).timestamp())


def pool_names(lines: list[str]) -> list[str]:
    """Номера серверов пула, которые прошивка выбрала за эти строки."""
    return [m.group(1) for line in lines for m in [NTP_NAME.search(line)] if m]


@pytest.fixture
def clock(stand: Stand, device_baseline: None) -> Iterator[Any]:
    """
    Сервер времени стенда поднят, устройство смотрит на него.

    После теста часы платы возвращаются к настоящим, а устройство - к пулу по
    умолчанию: иначе перекос в час и чужой адрес сервера уедут в соседние
    блоки, где метку времени сверяют с настоящей.
    """
    if not stand.clock.available():
        pytest.skip('на плате стенда нет сервера времени: нужен METF 6')

    stand.clock.start(board_epoch())
    stand.setup(ntp_server=stand.cfg.metf_host)
    try:
        yield stand.clock
    finally:
        stand.clock.drop(False)
        stand.clock.start(int(time.time()))
        stand.setup(ntp_server=DEFAULT_NTP_SERVER)
        stand.clock.stop()


def test_N1_manual_server_is_used(stand: Stand, clock: Any) -> None:
    """
    Заданный вручную сервер и используется, и время берётся его.

    Проверять по одному факту синхронизации мало: устройство могло взять время
    у кого угодно. Поэтому часы стенда сдвинуты на час, и сдвиг обязан
    приехать в посылке.
    """
    before = clock.requests_seen

    stand.reset_observers()
    stand.dut.press_button()      # ручное пробуждение синхронизацию форсирует
    session = stand.wait_session(timeout=180)

    assert clock.requests_seen > before, (
        'устройство не обратилось к серверу стенда\n' + session.text)
    assert 'NTP: Time synced' in session.text, (
        'синхронизация не состоялась\n' + session.text)

    assert session.payload is not None
    got = payload_epoch(session.payload)
    assert abs(got - board_epoch()) < SKEW_TOLERANCE, (
        f'в посылке {got}, а стенд отдавал около {board_epoch()}: '
        f'устройство взяло время не у нас')


def test_N2_falls_back_to_the_pool(stand: Stand, clock: Any) -> None:
    """
    Молчащий пользовательский сервер - не повод остаться без времени.

    Плата слушает, но не отвечает: так выглядит недоступный сервер в
    интернете. Прошивка обязана после неудачи пойти в пул.
    """
    clock.drop(True)

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180)

    assert 'NTP: No reply from NTP server' in session.text, (
        'сервер стенда молчал, а прошивка этого не заметила\n' + session.text)
    assert pool_names(session.lines), (
        'после неудачи устройство не попробовало пул\n' + session.text)


@pytest.mark.slow
def test_N3_pool_server_is_chosen_at_random(stand: Stand, clock: Any) -> None:
    """
    Сервер из пула выбирается случайно, а не по фиксированному кругу.

    Случайность - на сеанс: `get_next_ntp_server_id` берёт random только при
    первом обращении после загрузки, дальше идёт по кругу. Поэтому считаем
    первое имя каждого сеанса, а сеансов берём шесть: у четырёх серверов
    шесть одинаковых подряд - это один случай на тысячу.

    Резолв при этом может и не удаться: имя прошивка печатает в обоих случаях,
    и проверка от наличия интернета не зависит.
    """
    stand.setup(ntp_server=DEFAULT_NTP_SERVER)

    chosen = []
    for attempt in range(6):
        stand.reset_observers()
        stand.dut.press_button()
        session = stand.wait_session(timeout=180)
        names = pool_names(session.lines)
        assert names, f'сеанс {attempt + 1}: пул не опрашивался\n{session.text}'
        chosen.append(names[0])

    assert all(int(name) < NTP_POOL_SIZE for name in chosen), (
        f'номер сервера вне пула: {chosen}')
    assert len(set(chosen)) > 1, (
        f'шесть сеансов подряд выбрали один и тот же сервер: {chosen}. '
        f'random() не даёт случайности - проверьте, не вызван ли randomSeed')


@pytest.mark.slow
def test_N4_sync_is_not_asked_every_wakeup(stand: Stand, clock: Any) -> None:
    """
    После прогрева время спрашивается не каждое пробуждение.

    Первые NTP_WARMUP_SYNCS синхронизаций идут подряд намеренно - без пары
    измерений не посчитать поправку хода attiny. Дальше срок - сутки, то есть
    при периоде в две минуты 720 пробуждений, и трёх плановых сеансов подряд
    достаточно, чтобы утверждать «не каждое».

    Считаем именно плановые: кнопка синхронизацию форсирует независимо от
    срока (`sync_time.cpp`, maybe_sync_time), и сеанс по кнопке тут ничего не
    доказал бы.

    Период меняется до прогрева, а не после. Смена периода обнуляет и счётчик
    прогрева, и точку отсчёта (`config.cpp`, reset_period_min_tuned): прежнее
    измерение относилось к прежнему периоду и врёт. Прогрев перед сменой
    пропал бы впустую, и первые же плановые пробуждения снова синхронизировали
    бы время - по делу, а не по дефекту.
    """
    stand.setup(period_min=SHORT_PERIOD_MIN)
    try:
        for _ in range(NTP_WARMUP_SYNCS):
            stand.reset_observers()
            stand.dut.press_button()
            stand.wait_session(timeout=180)

        before = clock.requests_seen

        for number in range(3):
            session = stand.wait_session(timeout=300, mode=TRANSMIT_MODE)
            assert 'NTP: Time synced' not in session.text, (
                f'плановое пробуждение {number + 1} снова синхронизировало '
                f'время\n{session.text}')

        assert clock.requests_seen == before, (
            f'устройство ходило к серверу времени '
            f'{clock.requests_seen - before} раз за три плановых пробуждения')
    finally:
        stand.setup(period_min=120)


@pytest.mark.slow
def test_N5_unreachable_server_freezes_the_tuning(stand: Stand, clock: Any) -> None:
    """
    Недоступный сервер времени не двигает расписание.

    Поправку хода attiny прошивка считает только по времени от NTP
    (`config.cpp`: ветка tune_wakeup под `time_synced`). Между синхронизациями
    часы идут по оценке, и подстраивать период по ней значит подстраивать его
    под собственную ошибку - с каждым циклом всё сильнее.

    Поэтому при недоступном сервере проверяем ровно это: подстроенный период
    замер, счётчик неудач растёт, а часы продолжают идти по оценке, а не
    падают в 1970 год.

    Молчащего сервера стенда для этого мало: при неудаче своего сервера
    прошивка идёт в пул, а пул из сети стенда доступен, и синхронизация
    удаётся. Режем udp/123 целиком - посылка при этом доходит, иначе смотреть
    было бы не на что.
    """
    stand.reset_observers()
    stand.dut.press_button()
    start = stand.wait_session(timeout=180)
    assert start.payload is not None
    tuned = start.payload['period_min_tuned']
    stamp = payload_epoch(start.payload)

    with stand.net.ntp_down():
        failures = 0
        for _ in range(3):
            stand.reset_observers()
            stand.dut.press_button()
            session = stand.wait_session(timeout=180)
            assert session.payload is not None

            assert 'NTP: sync failed' in session.text, (
                'время взять было неоткуда, а прошивка сочла синхронизацию '
                'удачной\n' + session.text)
            failures += 1

            assert session.payload['period_min_tuned'] == tuned, (
                f'период подстроился по оценочному времени: было {tuned}, '
                f"стало {session.payload['period_min_tuned']}")
            assert session.payload['ntp_errors'] >= failures, (
                f"счётчик неудач {session.payload['ntp_errors']} "
                f'меньше числа неудач {failures}')

            now = payload_epoch(session.payload)
            assert now >= stamp, f'часы пошли назад: {stamp} -> {now}'
            stamp = now
