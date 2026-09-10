"""
Заводской сброс - блок R.

`POST /api/reset` из портала стирает EEPROM и конфигурацию Wi-Fi самой ЕСП, а
восстанавливает ровно одно - уникальный токен устройства (`config.cpp`,
factory_reset). Это и проверяется: токен пережил сброс, всё остальное вернулось
к умолчаниям `init_config`.

Здесь же живёт E10, и не по прихоти. Режим отпуска обязан работать на входе, у
которого вес импульса не задан, - в `alarm_interval_ticks` проверка vacation
стоит до проверки веса. Но состояние «вес не задан» настройками не создать:
«Авто» прошивка разрешает в конкретное число в момент применения
(`active_point_api.cpp`, applyInputParameter -> get_auto_factor), и в настройках
тройки не окажется никогда. После сброса вес не задан на обоих входах, и это
единственный момент, когда сценарий воспроизводим.

Модуль дорогой: сброс уносит сеть, адрес приёмника, брокер и период (по
умолчанию сутки), поэтому возврат стенда в рабочее состояние - часть модуля, а
не забота соседей. Тревога, поднятая в отпуске, снимается там же: при пороге
65535 условие снятия не выполняется никогда, а с нулевым порогом ветка снятия
недостижима вовсе (`Attiny85/src/alarm.h`, on_tick), так что снять её можно
только вернув каналу настоящий порог.
"""

from __future__ import annotations

import time
from typing import Any, Iterator

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
VACATION_INTERVAL = 65535   # UINT16_MAX: тревогой становится любой расход


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

    Порядок важен. Пороги считает ЕСП, и снять тревогу отпуска можно только
    после того, как каналу вернули настоящий вес, - иначе порог уедет в attiny
    нулём, а с нулевым порогом ветка снятия в on_tick недостижима.
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

    payload = stand.last_payload or {}
    if payload.get('alarm_flow0') or payload.get('alarm_flow1'):
        logger.info('снимаем тревогу, поднятую в режиме отпуска')
        stand.setup_alarms(channel=1, factor=10, alarm_flow=3600,
                           ctype=NAMUR, vacation=0)
        stand.reset_observers()
        cleared = stand.wait_session(timeout=600)
        assert cleared.payload['alarm_flow1'] is False, (
            f'тревога отпуска не снялась\n{cleared.text}')
        stand.setup(channel=1, alarm_flow=0)


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
    Всё остальное вернулось к умолчаниям init_config.

    Проверять по одному полю мало: сброс стирает EEPROM целиком, и частичный
    сброс выглядел бы как «одно поле не то», а означал бы, что стёрлось не всё.
    """
    payload = after_reset['payload']
    expected = {
        'f0': AS_COLD_CHANNEL,          # вес обоих входов - снова спецзначения
        'f1': AUTO_IMPULSE_FACTOR,
        'period_min': DEFAULT_WAKEUP_PERIOD_MIN,
        'ctype0': 0, 'ctype1': 0,       # типы входов
        'af0': 0, 'af1': 0,             # пороги тревог
        'al0': 0, 'al1': 0,
        'as0': 0, 'as1': 0,
        'vac': False, 'sc': False,
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
    и alarm_interval_ticks вернёт ноль, то есть «выключено». Отпуску считать
    нечего: тревогой объявлен любой импульс, и порог подменяется максимумом.
    Поэтому проверка типа входа и vacation стоит до проверки веса, и тест
    следит именно за этим порядком.
    """
    assert after_reset['payload']['f1'] == AUTO_IMPULSE_FACTOR, (
        'тест бессмысленен, если вес всё-таки задан')

    stand.setup(channel=1, ctype=NAMUR)
    on = stand.setup(vacation=1)
    assert on.payload['f1'] == AUTO_IMPULSE_FACTOR, 'вес не должен был появиться'
    assert on.alarm_config['interval1'] == VACATION_INTERVAL, (
        f'порог отпуска не уехал в attiny: {on.alarm_config}\n{on.text}')

    stand.reset_observers()
    # Минута между импульсами - расход, который ни один обычный порог тревогой
    # не считает. В отпуске считается любой.
    stand.dut.pulses(channel=1, count=2, gap=60.0)

    session = stand.wait_session(timeout=180, mode=ALARM_MODE)
    session.assert_alarm(flow1=1)

    stand.setup(vacation=0)
