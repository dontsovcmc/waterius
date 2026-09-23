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
from collections.abc import Iterator
from typing import Any
from urllib.parse import urlencode

import pytest
from loguru import logger

from . import portal as portal_mod
from .atboard import AtBoard, AtError
from .constants import COLD, NAMUR, REPO_ROOT, WATER_COLD
from .test_wifi import other_channel

pytestmark = [pytest.mark.stand, pytest.mark.portal, pytest.mark.slow]

DATA = REPO_ROOT / 'ESP8266' / 'data'


# Отличается и от базового веса стенда, и от спецзначений «Авто» (3) и «как у
# холодной» (7): иначе непонятно, что именно сохранилось
FACTOR = 25
READINGS = '12.345'
PERIOD_MIN = 60

# Импульсы на шаге определения счётчика: мастер обязан их досчитать
DETECT_PULSES = 3

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


# Куда `/api/connect_status` ведёт после подключения и после неудачи
# (`active_point_api.cpp`, get_api_connect_status). Кода ошибки портал не отдаёт
CONNECTED = '/input/1/setup.html'
NOT_CONNECTED = '/wifi_settings.html'

# Причину неудачи показывает страница Wi-Fi: прошивка подставляет номер строки
# strings.js в fill_tr_id (`active_point.cpp`, PARAM_WIFI_CONNECT_STATUS)
RE_CONNECT_STATUS = re.compile(r"fill_tr_id\('?(\d*)'?\s*,\s*'wifi_connect_status'\)")
WRONG_PASSWORD_CODE = '9'   # S_WL_WRONG_PASSWORD, «Ошибка подключения: Некорректный пароль»


def connect_status_code(board: AtBoard) -> str:
    """Номер строки, которой страница Wi-Fi объяснит пользователю неудачу."""
    answer = board.get(NOT_CONNECTED, portal_mod.HOST)
    assert answer.status == 200, f'{NOT_CONNECTED}: {answer.status}'
    match = RE_CONNECT_STATUS.search(answer.text)
    onload = re.search(r'<body[^>]*', answer.text)
    assert match, (f'на {NOT_CONNECTED} нет подстановки статуса подключения: '
                   f'{onload.group(0)[:200] if onload else answer.text[:200]}')
    return match.group(1)


def networks_list(board: AtBoard, timeout: float = 60.0) -> list[dict[str, Any]]:
    """Список сетей портала. Скан асинхронный: до его конца портал честно отдаёт пустой."""
    networks: list[dict[str, Any]] = []
    deadline = time.time() + timeout
    while time.time() < deadline and not networks:
        answer = board.get('/api/networks', portal_mod.HOST)
        assert answer.status == 200, answer.status
        networks = json.loads(answer.text or '[]')
        if not networks:
            time.sleep(3)
    assert networks, 'список сетей пуст: сканирование не дало результата'
    return networks


def start_connect(board: AtBoard) -> bool:
    """
    Начать подключение. Обрыв здесь - часть сценария: точка уходит на канал
    роутера (K2). Возвращает, был ли обрыв.
    """
    try:
        board.get('/api/start_connect?wizard=true', portal_mod.HOST)
    except AtError as err:
        logger.info(f'портал оборвал подключение, так и задумано: {err}')
        return True
    return False


def wait_redirect(board: AtBoard, want: set[str],
                  timeout: float = 120.0) -> tuple[str | None, bool]:
    """
    Итог подключения, как его увидит страница, и терялась ли по дороге точка.
    Пока идёт подключение, портал отвечает без адреса; с точки, переехавшей на
    канал роутера, AT-плата слетает - возвращаемся и спрашиваем снова, как это
    делает страница (K2).
    """
    redirect = None
    lost = False
    deadline = time.time() + timeout
    while time.time() < deadline and redirect not in want:
        try:
            redirect = api(board, '/api/connect_status').get('redirect')
        except AtError:
            lost = True
            logger.info('точка переехала на канал роутера, возвращаемся')
            time.sleep(3)
            board.join(board.portal_ssid)
            continue
        time.sleep(2)
    return redirect, lost


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
    networks = networks_list(board)
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

    start_connect(board)

    redirect, _ = wait_redirect(board, {CONNECTED})
    assert redirect == CONNECTED, (
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
    stand.dut.pulse(channel=COLD, count=DETECT_PULSES)
    time.sleep(2)
    after = api(board, f'/api/status/{COLD}')
    assert after['impulses'] - before['impulses'] == DETECT_PULSES, (
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


WRONG_PASSWORD = 'hil-wrong-password'


def test_W2_wrong_password_returns_to_wifi_settings(board: AtBoard, cfg: Any,
                                                    stand: Any) -> None:
    """
    Неверный пароль сети: мастер возвращает на страницу Wi-Fi, та называет
    причину - «Некорректный пароль», а не общую ошибку подключения, портал жив,
    и верный пароль после этого подключает (#282).

    Верный пароль сохраняется заново в любом исходе: с неверным устройство
    потеряло бы стенд до конца прогона.

    Заодно K1d: окно captive portal, открытое заново после неудачи, попадает
    на `/`, и ненастроенное устройство обязано показать там страницу ошибки
    (`active_point.cpp`, on_root), а настроенное - обычный вход.
    """
    ssid = stand.ap_ssid
    fresh = portal_mod.needs_setup(board)
    try:
        assert save(board, '/api/save_connect', ssid=ssid, password=WRONG_PASSWORD,
                    wizard='true') == {}
        start_connect(board)
        redirect, _ = wait_redirect(board, {CONNECTED, NOT_CONNECTED})
        assert redirect == NOT_CONNECTED, f'с неверным паролем мастер пошёл дальше: {redirect}'
        # Читается сразу: после удачного подключения подстановка уже другая
        code = connect_status_code(board)
        landing = portal_mod.landing_page(board, DATA)

        assert save(board, '/api/save_connect', ssid=ssid, password=cfg.ap_password,
                    wizard='true') == {}
        start_connect(board)
        redirect, _ = wait_redirect(board, {CONNECTED})
        assert redirect == CONNECTED, f'верный пароль после неверного не подключил: {redirect}'

        assert code == WRONG_PASSWORD_CODE, (
            f'с неверным паролем страница Wi-Fi покажет строку {code!r} из strings.js, '
            f'а не «Некорректный пароль» ({WRONG_PASSWORD_CODE})')
        if fresh is not None:
            want = portal_mod.LANDING_ERROR if fresh else portal_mod.LANDING_CONFIGURED
            assert landing == want, (
                f'после неверного пароля / отдал {landing or "не посадочную страницу"} '
                f'вместо {want}')
    finally:
        assert save(board, '/api/save_connect', ssid=ssid,
                    password=cfg.ap_password) == {}


def find_network(board: AtBoard, ssid: str,
                 timeout: float = 90.0) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    """
    Сеть по имени и последний увиденный список. Запрос после выдачи списка
    запускает новый скан (`active_point_api.cpp`, get_api_networks), так что
    повтор находит и сеть, поднявшуюся позже скана на старте портала.
    """
    seen: list[dict[str, Any]] = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        seen = networks_list(board, timeout=max(5.0, deadline - time.time()))
        ours = next((net for net in seen if net['ssid'] == ssid), None)
        if ours:
            return ours, seen
        time.sleep(3)
    return None, seen


def test_W5_router_on_another_channel(cfg: Any, stand: Any) -> None:
    """
    Роутер на другом канале, чем точка портала: на шаге подключения точка
    уходит на канал роутера, телефон теряет её, возвращается - и мастер доходит
    до конца (K2).

    Точка поднимается на канале из настроек, то есть прошлого подключения
    (`active_point.cpp`, ap_channel), поэтому канал роутера меняется до входа
    в портал.
    """
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    payload = stand.last_payload or {}
    assert 'channel' in payload, 'канал роутера узнать не из чего: нет посылки со связью'
    old = int(payload['channel'])
    new = other_channel(old)
    ssid = stand.ap_ssid

    with stand.router.channel(new):
        router_channel = stand.router.config().get('channel')
        ap_on = stand.router.ap_enabled()
        assert router_channel == str(new) and ap_on, (
            f'роутер не перешёл на канал {new}: канал {router_channel}, точка {ap_on}')

        with portal_mod.session(cfg, stand) as board:
            started = find_line(stand, portal_mod.RE_AP_STARTED)
            assert started, 'нет строки о запуске точки портала'
            assert int(started.group(1)) == old, (
                f'точка портала поднялась на канале {started.group(1)}, а не на канале '
                f'прошлого подключения {old}: сценарий со сменой канала не воспроизведён')

            ours, seen = find_network(board, ssid)
            assert ours, (
                f'сети стенда {ssid} нет в списке и после повторных сканов: '
                f'{[(net["ssid"], net["wifi_channel"]) for net in seen]}; '
                f'роутер: канал {stand.router.config().get("channel")}, '
                f'точка {stand.router.ap_enabled()}')
            assert int(ours['wifi_channel']) == new, (
                f"в списке сеть стенда на канале {ours['wifi_channel']}, роутер на {new}")

            answer = portal_mod.post_json(board, '/api/save_connect', ssid=ssid,
                                          password=cfg.ap_password,
                                          wifi_channel=ours['wifi_channel'],
                                          bssid=ours['bssid'], wizard='true')
            assert not answer.get('errors'), answer
            # error=0 - S_ANOTHER_CHANNEL: страница подключения заранее предупредит,
            # что телефон может потерять связь с Ватериусом
            assert 'error=0' in answer.get('redirect', ''), (
                f'смена канала не отмечена, предупреждения не будет: {answer}')
            dropped = start_connect(board)
            redirect, lost = wait_redirect(board, {CONNECTED})

            assert dropped or lost, (
                f'AT-плата ни разу не потеряла точку: точка не ушла с канала {old} на {new}')
            assert redirect == CONNECTED, (
                f'после смены канала мастер не увидел подключения: {redirect}')


def hex_digits(mac: str) -> str:
    return re.sub(r'[^0-9a-f]', '', mac.lower())


def test_W1b_networks_list(board: AtBoard, stand: Any) -> None:
    """
    Список сетей: у сети стенда канал и производитель роутера, повторов нет
    (#382, #281).

    #382: в строки списка уходил канал самой ЕСП, а не сети, и быстрый коннект
    после мастера промахивался. Эталон - последняя посылка: `channel` роутера
    и `router_mac`, в котором прошивка оставляет первые три октета. Два
    запроса подряд - #281: перезагрузка страницы удваивала список.

    Сверку каждой строки с эфиром дала бы AT-плата (`AT+CWLAP`), но формат её
    ответа в репозитории не записан; сверяется то, что стенд знает сам.
    """
    payload = stand.last_payload or {}
    assert 'channel' in payload and 'router_mac' in payload, (
        'канал роутера узнать не из чего: нет посылки со связью')

    first = networks_list(board)
    ours = [net for net in first if net['ssid'] == stand.ap_ssid]
    assert len(ours) == 1, f'сеть стенда в списке {len(ours)} раз: {first}'
    assert int(ours[0]['wifi_channel']) == int(payload['channel']), (
        f"канал в списке {ours[0]['wifi_channel']}, роутер на {payload['channel']}")
    assert hex_digits(ours[0]['bssid'])[:6] == hex_digits(payload['router_mac'])[:6], (
        f"BSSID {ours[0]['bssid']} не того роутера: {payload['router_mac']}")

    for listing in (first, networks_list(board)):
        bssids = [hex_digits(net['bssid']) for net in listing]
        assert len(bssids) == len(set(bssids)), f'сети повторяются: {listing}'
