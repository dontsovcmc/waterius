"""
Заводской сброс - блок R.

`POST /api/reset` из портала стирает EEPROM и конфигурацию Wi-Fi самой ЕСП, а
восстанавливает ровно одно - уникальный токен устройства (`config.cpp`,
factory_reset). Это и проверяется: токен пережил сброс, всё остальное вернулось
к умолчаниям `init_config`.

Здесь же живут E10 и A10, и не по прихоти. Обоим нужен вход, у которого вес
импульса не задан: режим отпуска обязан без веса работать (`core/alarm.cpp`,
alarm_thresholds: vacation проверяется до веса), а «Авто» только незаданный
вес и определяет. Но состояние «вес не задан» настройками не создать: «Авто»
прошивка разрешает в конкретное число в момент применения
(`active_point_api.cpp`, applyInputParameter -> get_auto_factor), и в настройках
тройки не окажется никогда. После сброса вес не задан на обоих входах, и это
единственный момент, когда оба сценария воспроизводимы. A10 идёт последним:
он вес задаёт.

Модуль дорогой: сброс уносит сеть, адрес приёмника, брокер и период (по
умолчанию сутки), поэтому возврат стенда в рабочее состояние - часть модуля, а
не забота соседей. Тревогу, поднятую в отпуске, снимает выключение режима
(`active_point_api.cpp`, save_vacation) - им E10 и заканчивается.
"""

from __future__ import annotations

import json
import time
from typing import Any, Iterator
from urllib.parse import urlencode

import pytest
from loguru import logger

from . import portal as portal_mod
from .atboard import AtBoard
from .logwatch import ALARM_MODE

pytestmark = [pytest.mark.stand, pytest.mark.reset, pytest.mark.slow,
              pytest.mark.requires(attiny=41)]

NAMUR = 0
AUTO_IMPULSE_FACTOR = 3     # core/types.h
AS_COLD_CHANNEL = 7         # core/types.h
DEFAULT_WAKEUP_PERIOD_MIN = 1440
VACATION_PULSES = 1         # в отпуске порог объёма - один импульс

CHANNEL = 1                 # вход холодной воды: «Авто» живёт у него
# core/readings.h: IMPULS_LIMIT_1. До стольких импульсов включительно «Авто»
# даёт 10 л/имп, дальше - 1 л/имп
AUTO_LIMIT = 3
FACTOR_FEW = 10
FACTOR_MANY = 1


def _wait_portal(stand: Any, timeout: float = 90.0) -> str:
    """Дождаться точки доступа портала и вернуть её имя."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        stand.log.poll()
        ssid = portal_mod.find_ap(stand.log.lines)
        if ssid:
            return ssid
        time.sleep(1)
    pytest.fail(f'точка доступа портала не поднялась за {timeout:.0f} с')


def _post(board: AtBoard, path: str, **params: Any) -> dict[str, Any]:
    """Форма портала: поля телом, как их шлёт страница. Ответ целиком."""
    answer = board.post(path, portal_mod.HOST, body=urlencode(params).encode())
    assert answer.status == 200, f'{path}: код {answer.status}'
    return json.loads(answer.text or '{}')


def _status(board: AtBoard) -> dict[str, Any]:
    """Состояние входа - то, что показывает страница определения счётчика."""
    answer = board.get(f'/api/status/{CHANNEL}', portal_mod.HOST)
    assert answer.status == 200, f'/api/status/{CHANNEL}: код {answer.status}'
    body = json.loads(answer.text or '{}')
    assert 'error' not in body, f'нет связи с attiny: {body}'
    return body


def _wait_impulses(board: AtBoard, want: int, timeout: float = 10.0) -> dict[str, Any]:
    """Дождаться, пока вход насчитает want импульсов с начала сеанса настройки."""
    deadline = time.time() + timeout
    while True:
        body = _status(board)
        if body['impulses'] >= want or time.time() >= deadline:
            return body
        time.sleep(1)


@pytest.fixture(scope='module')
def after_reset(cfg: Any, stand: Any) -> Iterator[dict]:
    """
    Устройство после заводского сброса, снова в сети стенда.

    Сеть и адрес приёмника возвращаются сразу: без них не будет ни посылки, ни
    следующего теста. Всё остальное - вес, период, типы входов, брокер -
    намеренно остаётся сброшенным, это и есть предмет проверки.
    """
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    assert cfg.ap_password, '[router] ap_password в stand.ini'

    stand.reset_observers()
    stand.dut.press_button()
    before = stand.wait_session(timeout=180).payload
    assert before and before.get('key'), 'в посылке нет токена, сверять нечего'
    logger.info(f'до сброса: key={before["key"]}, f0={before.get("f0")}, '
                f'f1={before.get("f1")}, period_min={before.get("period_min")}')

    ssid = stand.cfg.ap_ssid or stand.router.config().get('ssid', '')
    assert ssid, 'не удалось узнать имя точки доступа стенда'

    stand.log.clear()
    stand.dut.hold_button()
    board = AtBoard(cfg.atboard_port)
    try:
        logger.info(f'AT-плата в сети портала {_wait_portal(stand)}: '
                    f'{board.join(portal_mod.find_ap(stand.log.lines))}')

        answer = board.post('/api/reset', portal_mod.HOST)
        assert answer.status == 200, f'/api/reset: код {answer.status}'
        logger.info('сброс отправлен, ждём перезапуск')

        # После сброса ЕСП перезагружается и поднимает портал заново
        # (init_config ставит SETUP_MODE), но точка та же и AT-плата в неё уже
        # не подключена: ассоциация умерла вместе с перезапуском.
        stand.log.clear()
        again = _wait_portal(stand, timeout=120)
        logger.info(f'портал после сброса: {again}, адрес платы {board.join(again)}')

        stand.reset_observers()
        portal_mod.configure(board, ssid, cfg.ap_password, cfg.http_url)
    finally:
        board.close()

    session = stand.wait_session(timeout=300)
    assert session.payload is not None, (
        f'после сброса и настройки посылка не дошла\n{session.text}')
    logger.info(f'после сброса: f0={session.payload.get("f0")}, '
                f'f1={session.payload.get("f1")}, '
                f'period_min={session.payload.get("period_min")}')

    try:
        yield {'before': before, 'session': session, 'payload': session.payload}
    finally:
        _restore(stand, before)


def _restore(stand: Any, before: dict) -> None:
    """
    Вернуть стенд в рабочее состояние: сеть, почта, брокер, эталон настроек.

    Тревога, оставшаяся от E10, снимается кнопкой: сама она не гаснет.
    """
    logger.info('возвращаем стенд в рабочее состояние после сброса')
    # Почта уезжает в облако вместе с показаниями, и без неё блок G сверял бы
    # ответ облака, а не прошивку. Имя параметра - waterius_email: в посылке
    # поле называется короче (json.cpp), и по нему прошивка его не примет.
    email = before.get('email')
    if email:
        try:
            stand.setup(waterius_email=email)
        except AssertionError as err:
            logger.warning(f'почта не вернулась: {err}')

    stand.ensure_baseline()          # вес, период, типы входов, квитанции
    stand.ensure_mqtt()              # брокер: mqtt_on, адрес и топик стенда

    # Автодискавери сброс включает (init_config, MQTT_AUTO_DISCOVERY), а в
    # BASELINE его нет: тесты команд поднимают его сами фикстурой discovery_on.
    # Возвращаем как было, чтобы модуль не оставлял следов.
    was_ha = bool(before.get('ha'))
    if bool((stand.last_payload or {}).get('ha')) != was_ha:
        stand.setup(mqtt_auto_discovery=int(was_ha))

    # Тревога отпуска сама не гаснет: снимает её кнопка, она же и привозит
    # посылку, по которой видно результат.
    payload = stand.last_payload or {}
    if payload.get('alarm_flow0') or payload.get('alarm_flow1'):
        logger.info('снимаем тревогу, поднятую в режиме отпуска')
        stand.reset_observers()
        stand.dut.press_button()
        cleared = stand.wait_session(timeout=600)
        assert cleared.payload['alarm_flow1'] is False, (
            f'тревога отпуска не снялась\n{cleared.text}')


def test_R1_token_survives_reset(after_reset: dict) -> None:
    """
    Токен - единственное, что переживает сброс.

    Им устройство опознаётся в облаке, и потерять его значит потерять привязку
    к аккаунту: показания поедут в никуда, а владелец узнает об этом не сразу.
    """
    was = after_reset['before']['key']
    now = after_reset['payload']['key']
    assert now == was, f'токен изменился: был {was}, стал {now}'


def test_R2_settings_return_to_defaults(after_reset: dict) -> None:
    """
    Всё остальное вернулось к умолчаниям.

    Список широкий намеренно. Сброс возвращает структуру настроек целиком, и
    проверка по двум-трём полям зеленела бы на любом частичном сбросе: пороги
    тревог, показания, серийные номера и маска квитанции лежат в тех же
    настройках и сбрасываться обязаны так же.

    Типы входов сюда же, хотя живут они в EEPROM attiny: сбросить их ЕСП может
    только командой, и без неё «заводское состояние» осталось бы с датчиком
    протечки на входе.
    """
    payload = after_reset['payload']
    expected = {
        'f0': AS_COLD_CHANNEL,          # вес обоих входов - снова спецзначения
        'f1': AUTO_IMPULSE_FACTOR,
        'period_min': DEFAULT_WAKEUP_PERIOD_MIN,
        'ctype0': 0, 'ctype1': 0,       # типы входов - в EEPROM attiny
        'ch0_start': 0, 'ch1_start': 0,  # показания
        'serial0': '', 'serial1': '',
        'company': '', 'place': '',
        'av0': 0, 'av1': 0,             # пороги тревог
        'ar0': 0, 'ar1': 0,
        'ah0': 0, 'ah1': 0,
        'as0': 0, 'as1': 0,
        'vac': False, 'sc': False,
        'ackw': False, 'ackh': False,   # маска квитанции - CONFIRM_ANY
        'ackm': False,
        'voltage_cal': 100,
        'mqtt': False,                  # брокер выключен, адрес стенда забыт
        'ha': False,
    }
    wrong = {name: payload.get(name) for name, want in expected.items()
             if payload.get(name) != want}
    assert not wrong, f'после сброса осталось не умолчание: {wrong}'


def test_E10_vacation_works_without_factor(after_reset: dict, stand: Any) -> None:
    """
    Режиму «Я уехал» известный вес импульса не нужен.

    Обычный порог без веса посчитать нельзя - литры на импульс взять неоткуда,
    и пересчёт вернёт ноль, то есть «выключено». Отпуску считать нечего:
    тревогой объявлен любой импульс, и порог объёма подменяется единицей.
    Поэтому проверка типа входа и vacation стоит до проверки веса, и тест
    следит именно за этим порядком.
    """
    assert after_reset['payload']['f1'] == AUTO_IMPULSE_FACTOR, (
        'тест бессмысленен, если вес всё-таки задан')

    stand.setup(channel=1, ctype=NAMUR)
    on = stand.setup(vacation=1)
    assert on.payload['f1'] == AUTO_IMPULSE_FACTOR, 'вес не должен был появиться'
    assert on.alarm_config['vol1'] == VACATION_PULSES, (
        f'порог отпуска не уехал в attiny: {on.alarm_config}\n{on.text}')

    stand.reset_observers()
    # Минута между импульсами - расход, который ни один обычный порог тревогой
    # не считает. В отпуске считается любой.
    stand.dut.pulses(channel=1, count=2, gap=60.0)

    session = stand.wait_session(timeout=180, mode=ALARM_MODE)
    session.assert_alarm(flow1=1)

    stand.setup(vacation=0)


def test_A10_auto_factor_by_pulse_count(after_reset: dict, cfg: Any,
                                        stand: Any) -> None:
    """
    «Авто» выбирает вес импульса по числу импульсов при настройке (#69, #78, #339).

    До трёх импульсов включительно - 10 л/имп, от четырёх - 1 л/имп
    (`core/readings.cpp`, get_auto_factor). Считаются импульсы с начала сеанса
    настройки. Проверяются обе стороны порога: один импульс и четыре.

    Итог виден дважды: подсказкой `/api/status/1` - её показывает страница
    определения счётчика, - и весом в посылке после сохранения. Сохраняется
    один раз, на четырёх: заданный вес «Авто» уже не перетирает, функция
    смотрит на сохранённый вес, а не на присланный.

    Там же #346: пока вес не задан, выбор типа входа ведёт на определение
    счётчика. Обратное - повторная настройка сразу к показаниям - проверяет W1.
    """
    now = (stand.last_payload or after_reset['payload']).get('f1')
    assert now == AUTO_IMPULSE_FACTOR, f'вес уже задан ({now}): определять нечего'

    with portal_mod.session(cfg, stand) as board:
        answer = _post(board, '/api/save_input_type', input=CHANNEL, ctype=NAMUR)
        assert not answer.get('errors'), answer
        assert answer.get('redirect') == f'/input/{CHANNEL}/detect.html', (
            f'первая настройка минует определение счётчика: {answer}')

        start = _status(board)
        assert start['impulses'] == 0, (
            f"до первого импульса вход уже насчитал {start['impulses']}: "
            'граница порога сдвинута, проверка потеряла бы смысл')

        stand.dut.pulse(channel=CHANNEL, count=1)
        one = _wait_impulses(board, 1)
        assert one['impulses'] == 1, f'импульс не досчитался: {one}'
        assert one['factor'] == FACTOR_FEW, (
            f"один импульс - это {FACTOR_FEW} л/имп, подсказка {one['factor']}")

        stand.dut.pulse(channel=CHANNEL, count=AUTO_LIMIT)
        many = _wait_impulses(board, AUTO_LIMIT + 1)
        assert many['impulses'] == AUTO_LIMIT + 1, f'импульсы не досчитались: {many}'
        assert many['factor'] == FACTOR_MANY, (
            f"{AUTO_LIMIT + 1} импульса - это {FACTOR_MANY} л/имп, "
            f"подсказка {many['factor']}")

        answer = _post(board, '/api/save', input=CHANNEL, factor=AUTO_IMPULSE_FACTOR)
        assert not answer.get('errors'), answer

    stand.reset_observers()
    stand.dut.press_button()
    session = stand.wait_session(timeout=180)
    assert session.payload is not None, f'после настройки нет посылки\n{session.text}'
    assert session.payload['f1'] == FACTOR_MANY, (
        f"сохранён вес {session.payload['f1']}, а «Авто» при {AUTO_LIMIT + 1} "
        f'импульсах обязан дать {FACTOR_MANY}')
