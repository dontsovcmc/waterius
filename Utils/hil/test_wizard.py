"""
Мастер первичной настройки, серверная часть - блок W (A1-A9 ручного плана).

Стенд проходит мастер теми же запросами, что делают его страницы: список
сетей, сохранение сети, подключение, тип входа, определение счётчика,
показания с весом, настройка отправки, выход. Браузера здесь нет и не будет -
проверяется не разметка, а то, что прошивка принимает шаги мастера в их
порядке и что после мастера устройство действительно выходит на связь с
заданными настройками.

Разметку, переходы между страницами и поведение без JavaScript проверяет
симулятор (`simulator/test_simulator.js`, сценарий мастера).

Сеть в мастере указывается та же, в которой устройство и так живёт: тест
проходит настоящий путь, но не может увести стенд в чужой эфир. Вместе с
сетью уходит пара «канал + BSSID» из скана, как её шлёт страница: без неё
первый коннект идёт полным сканом эфира (#340, #382).
"""

from __future__ import annotations

import json
import re
import time
from typing import Any, Iterator
from urllib.parse import urlencode

import pytest
from loguru import logger

from . import portal as portal_mod
from .atboard import AtBoard, AtError
from .constants import COLD, NAMUR, WATER_COLD

pytestmark = [pytest.mark.stand, pytest.mark.portal, pytest.mark.slow]


# Отличается и от базового веса стенда, и от спецзначений «Авто» (3) и «как у
# холодной» (7): иначе непонятно, что именно сохранилось
FACTOR = 25
READINGS = '12.345'
PERIOD_MIN = 60

PULSES = 3

# save_fast_connect печатает пару, которую сохранил
RE_FAST_CONNECT = re.compile(r'Fast connect: channel=(\d+) bssid=(\S+)')


def api(board: AtBoard, path: str, **params: Any) -> dict[str, Any]:
    query = f'?{urlencode(params)}' if params else ''
    answer = board.get(f'{path}{query}', portal_mod.HOST)
    assert answer.status == 200, f'{path}: {answer.status}'
    return json.loads(answer.text or '{}')


def save(board: AtBoard, path: str, **params: Any) -> dict[str, str]:
    """Сохранить форму и вернуть ошибки полей."""
    body = portal_mod.post_json(board, path, **params)
    return {name: str(code) for name, code in (body.get('errors') or {}).items()}


def find_line(stand: Any, pattern: re.Pattern[str],
              timeout: float = 5.0) -> re.Match[str] | None:
    """Строка лога по шаблону: METF отдаёт UART с задержкой, ждём немного."""
    deadline = time.time() + timeout
    while True:
        stand.log.poll()
        for line in stand.log.lines:
            match = pattern.search(line)
            if match:
                return match
        if time.time() >= deadline:
            return None
        time.sleep(0.5)


@pytest.fixture
def board(cfg: Any, stand: Any) -> Iterator[AtBoard]:
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    with portal_mod.session(cfg, stand) as device:
        yield device


def test_W1_wizard_configures_the_device(board: AtBoard, cfg: Any,
                                         stand: Any) -> None:
    """
    Мастер от списка сетей до первой посылки.

    Каждый шаг проверяется своим утверждением, а в конце - тем, ради чего
    мастер и нужен: устройство подключилось к сети и прислало показания с
    теми весом, типом входа и периодом, которые в него вводили.
    """
    ssid = stand.ap_ssid

    # A3: список сетей. Скан идёт асинхронно, и до его конца портал честно
    # отдаёт пустой список
    networks: list[dict[str, Any]] = []
    deadline = time.time() + 60
    while time.time() < deadline and not networks:
        answer = board.get('/api/networks', portal_mod.HOST)
        assert answer.status == 200, answer.status
        networks = json.loads(answer.text or '[]')
        if not networks:
            time.sleep(3)
    assert networks, 'список сетей пуст: сканирование не дало результата'
    assert any(net['ssid'] == ssid for net in networks), (
        f'сети стенда {ssid} нет в списке: {[n["ssid"] for n in networks]}')

    # Поля, без которых страница не соберёт форму: уровень рисует иконку, пара
    # канал-BSSID уезжает в скрытые поля и даёт коннект без полного скана.
    # Сортировка по уровню - дело страницы (common.js, getWifiList), и
    # проверяет её браузерный тест симулятора
    for net in networks:
        assert 1 <= net['level'] <= 4, net
        assert net['bssid'] and net['wifi_channel'], net

    # A4: сеть и пароль, с парой канал-BSSID из скана - как их шлёт страница
    ours = next(net for net in networks if net['ssid'] == ssid)
    assert save(board, '/api/save_connect', ssid=ssid,
                password=cfg.ap_password, wifi_channel=ours['wifi_channel'],
                bssid=ours['bssid'], wizard='true') == {}

    fast = find_line(stand, RE_FAST_CONNECT)
    assert fast, ('прошивка не сохранила канал и BSSID: первый коннект пойдёт '
                  'полным сканом эфира (#340, #382)')
    assert (int(fast.group(1)), fast.group(2).lower()) == (
        int(ours['wifi_channel']), ours['bssid'].lower()), (
        f'сохранена чужая пара: {fast.group(0)}, из скана {ours}')

    # Точка доступа переезжает на канал роутера, и клиент с неё слетает - у
    # человека это видно как «нет связи с Ватериусом». Возвращаться обязан
    # сам, поэтому обрыв здесь не ошибка, а часть сценария (K2)
    try:
        board.get('/api/start_connect?wizard=true', portal_mod.HOST)
    except AtError as err:
        logger.info(f'портал оборвал подключение, так и задумано: {err}')

    redirect = None
    deadline = time.time() + 120
    while time.time() < deadline and redirect != '/input/1/setup.html':
        try:
            redirect = api(board, '/api/connect_status').get('redirect')
        except AtError:
            logger.info('точка переехала на канал роутера, возвращаемся')
            time.sleep(3)
            board.join(board.portal_ssid)
            continue
        time.sleep(2)
    assert redirect == '/input/1/setup.html', (
        f'мастер не увидел подключения к сети: {redirect}')

    # A5: тип входа. Вес у стенда уже задан, и повторная настройка обязана
    # вести сразу к показаниям, минуя определение счётчика (#346)
    answer = portal_mod.post_json(board, '/api/save_input_type', input=COLD, ctype=NAMUR)
    assert not answer.get('errors'), answer
    assert answer.get('redirect') == f'/input/{COLD}/settings.html', (
        f'повторная настройка ведёт не к показаниям: {answer}')

    # A6: страница определения счётчика считает импульсы вживую
    before = api(board, f'/api/status/{COLD}')
    assert 'error' not in before, f'нет связи с attiny: {before}'
    stand.dut.pulse(channel=COLD, count=PULSES)
    time.sleep(2)
    after = api(board, f'/api/status/{COLD}')
    assert after['impulses'] - before['impulses'] == PULSES, (
        f"импульсы не досчитались: было {before['impulses']}, "
        f"стало {after['impulses']}")

    # A7: показания и вес импульса
    assert save(board, '/api/save', input=COLD, cname=WATER_COLD,
                channel_start=READINGS, factor=FACTOR) == {}

    # A9: куда отправлять
    assert save(board, '/api/save', input=COLD, waterius_on=1, http_on=1,
                http_url=cfg.http_url, period_min=PERIOD_MIN) == {}

    logger.info('мастер пройден, ждём первую посылку')

    # Очередь приёмника чистим прямо перед выходом: в ней лежат посылки,
    # присланные до мастера, и последняя из них выглядит как результат
    # настройки, хотя настройки в ней прежние
    stand.reset_observers()
    board.get('/api/turnoff', portal_mod.HOST)

    session = stand.wait_session(timeout=300)
    payload = session.payload
    assert payload is not None, 'после мастера устройство не прислало показания'
    assert payload['f1'] == FACTOR, payload['f1']
    assert payload['ctype1'] == NAMUR, payload['ctype1']
    assert payload['period_min'] == PERIOD_MIN, payload['period_min']
    assert abs(float(payload['ch1']) - float(READINGS)) < 0.001, payload['ch1']
