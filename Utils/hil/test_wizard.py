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
from .atboard import AtBoard, AtError, Network
from .constants import COLD, NAMUR, REPO_ROOT, WATER_COLD
from .test_wifi import other_channel

pytestmark = [pytest.mark.stand, pytest.mark.portal]

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

# Чем кончилось подключение, словами самой прошивки (`wifi_helpers.cpp`, wifi_connect)
RE_WIFI_CONNECTED = re.compile(r'WIFI: SSID: (\S+) Channel: (\d+)')

# W5: сколько ждать, пока точка портала уведёт станцию за собой, и сколько -
# пока она сама встанет на канал роутера. Оба срока - от начала подключения к
# роутеру, а оно идёт полным сканом эфира, если быстрый коннект промахнулся
KICK_S = 20.0
MOVE_S = 90.0

# W6: сколько ждать подтверждения, что точка осталась на своём канале. Ждать
# тут нечего - точка уже в эфире, - но скан во время подключения ЕСП может
# разойтись с ней разок
STAY_S = 30.0


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
    deadline = time.monotonic() + timeout
    while True:
        stand.log.poll()
        for line in stand.log.lines:
            match = pattern.search(line)
            if match:
                return match
        if time.monotonic() >= deadline:
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
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and not networks:
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
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and redirect not in want:
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


def find_network(board: AtBoard, ssid: str, channel: int | None = None,
                 timeout: float = 90.0) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    """
    Сеть по имени и последний увиденный список. Запрос после выдачи списка
    запускает новый скан (`active_point_api.cpp`, get_api_networks), так что
    повтор находит и сеть, поднявшуюся позже скана на старте портала.

    Канал, если он задан, ждём наравне с именем: список, снятый до переезда
    роутера, честно показывает прежний, и по одному имени тест взял бы
    устаревшую строку и пошёл настраивать сеть на канале, которого уже нет.
    """
    seen: list[dict[str, Any]] = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        seen = networks_list(board, timeout=max(5.0, deadline - time.monotonic()))
        ours = next((net for net in seen if net['ssid'] == ssid
                     and (channel is None or int(net['wifi_channel']) == channel)), None)
        if ours:
            return ours, seen
        time.sleep(3)
    return None, seen


def portal_in_air(board: AtBoard, ssid: str, channel: int,
                  timeout: float = MOVE_S) -> tuple[Network | None, list[Network]]:
    """
    Дождаться точку портала в эфире на нужном канале и вернуть её и весь эфир.

    Скан повторяется: пока ЕСП подключается к роутеру, её точка то пропадает,
    то возвращается, и один скан ничего не доказывает. Эфир отдаётся целиком -
    без него отказ не скажет, где точка осталась. Пустой скан картину не
    затирает: он значит лишь, что в этот миг плата не слышала никого.
    """
    seen: list[Network] = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            снимок = board.scan()
        except AtError as err:
            logger.info(f'скан эфира не удался, повторяю: {err}')
            time.sleep(2)
            continue
        seen = снимок or seen
        ours = [net for net in снимок if net.ssid == ssid]
        for net in ours:
            if net.channel == channel:
                return net, seen
        if ours:
            logger.info(f'точка портала пока на канале {ours[0].channel}')
        time.sleep(2)
    return None, seen


def test_W5_router_on_another_channel(cfg: Any, stand: Any) -> None:
    """
    Роутер на другом канале, чем точка портала (K2).

    Радио у ЕСП одно: подключаясь к роутеру, она уводит на его канал и свою
    точку, а клиента при этом теряет. Телефон обязан пережить это сам -
    отключиться, найти точку заново уже на новом канале и вернуться в мастер.
    Тем же путём идёт и стенд: `AT+CWSTATE?` показывает потерю сети, `AT+CWLAP`
    находит точку на канале роутера, `AT+CWJAP` возвращает плату.

    Скан эфира здесь не удобство, а единственное прямое доказательство: о
    переезде точки прошивка не печатает ничего.

    Точка поднимается на канале из настроек, то есть прошлого подключения
    (`active_point.cpp`, ap_channel), поэтому канал роутера меняется до входа
    в портал.
    """
    if not cfg.atboard_port:
        pytest.skip('нет AT-платы: [atboard] port в stand.ini')
    payload = stand.last_payload or {}
    assert 'channel' in payload, 'канал роутера узнать не из чего: нет посылки со связью'
    old = int(payload['channel'])
    new = other_channel(cfg, old)
    ssid = stand.ap_ssid

    with stand.router.channel(new):
        router_channel = stand.router.config().get('channel')
        ap_on = stand.router.ap_enabled()
        assert router_channel == str(new) and ap_on, (
            f'роутер не перешёл на канал {new}: канал {router_channel}, точка {ap_on}')

        with portal_mod.session(cfg, stand) as board:
            portal_ssid = board.portal_ssid
            assert portal_ssid, 'имя точки портала неизвестно: возвращаться некуда'
            started = find_line(stand, portal_mod.RE_AP_STARTED)
            assert started, 'нет строки о запуске точки портала'
            assert int(started.group(1)) == old, (
                f'точка портала поднялась на канале {started.group(1)}, а не на канале '
                f'прошлого подключения {old}: сценарий со сменой канала не воспроизведён')

            ours, seen = find_network(board, ssid, channel=new)
            assert ours, (
                f'сети стенда {ssid} на канале {new} нет в списке и после '
                f'повторных сканов: {[(net["ssid"], net["wifi_channel"]) for net in seen]}; '
                f'роутер: канал {stand.router.config().get("channel")}, '
                f'точка {stand.router.ap_enabled()}')

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

            # Переезд точки прошивка не печатает: радио у ЕСП одно, и при
            # подключении к роутеру точка уходит следом молча
            # (`active_point.cpp`, ap_channel). Улику стенд берёт из эфира сам:
            # станция остаётся без сети, а скан находит имя точки на канале
            # роутера. Обрыв запроса уликой не годится - транспорт AT-платы
            # после него молча возвращается и повторяет запрос
            # (`atboard.py`, _recover), так что до теста обрыв может и не дойти
            kicked = board.wait_offline(KICK_S)
            logger.info(f'после start_connect: запрос оборвался {dropped}, '
                        f'станция вне сети {kicked}')

            moved, air = portal_in_air(board, portal_ssid, new, MOVE_S)
            assert moved, (
                f'точка портала {portal_ssid} не встала на канал {new} за '
                f'{MOVE_S:.0f} с: переезда не было. В эфире '
                f'{[(net.ssid, net.channel) for net in air]}; запрос оборвался '
                f'{dropped}, станция вне сети {kicked}, роутер на канале '
                f'{stand.router.config().get("channel")}')
            logger.info(f'точка портала переехала на канал {moved.channel}, '
                        f'возвращаемся на неё')

            # Возвращается клиент сам: Ватериус его отключил и звать не будет.
            # Имя то же, а канал и BSSID новые - их ищет сам модуль по имени
            board.join(portal_ssid)
            redirect, lost = wait_redirect(board, {CONNECTED})
            assert redirect == CONNECTED, (
                f'после смены канала мастер не увидел подключения: {redirect}; '
                f'по дороге точка терялась снова {lost}')


@pytest.mark.reset
def test_W6_router_on_the_same_channel(fresh_device: Any, cfg: Any,
                                       stand: Any) -> None:
    """
    Роутер на том же канале, что и точка портала: переезжать точке некуда.

    Обратная сторона W5 и настоящая первая настройка: сети устройство не знает,
    поэтому точка портала поднимается на заводском канале (`core/wifi.cpp`,
    ap_channel - канал из настроек, а после сброса он равен 1). Роутер уводим
    туда же. Тогда страница подключения не должна пугать предупреждением о
    потере связи (S_ANOTHER_CHANNEL), точка обязана остаться на своём канале, а
    мастер - дойти до конца.

    Связь при этом всё равно рвётся - один раз, на самом подключении. Это не
    переезд: `active_point.cpp` зовёт `wifi_connect(sett, WIFI_AP_STA)`, а тот
    в `wifi_begin` (`wifi_helpers.cpp`) делает `WiFi.disconnect(true)` и заново
    ставит режим, пересоздавая точку при любом канале. Замер 28.09, два прогона
    из двух:

        00:55:104  Start connect
        00:55:208  WIFI: disconnect          <- здесь AT-плату выбивает
        00:55:842  WIFI: begin channel: 1
        00:57:671  WIFI: Connected. SSID: waterius_stand Channel: 1

    Поэтому обрыв тут не приговор, а мерка: ровно один эпизод, и телефон
    возвращается сам. Ноль будет значить, что прошивка перестала ронять клиента,
    и проверку надо пересмотреть; два - что его роняют повторно. Считаются
    именно эпизоды: пока точка лежит, транспорт возвращается по нескольку раз
    подряд, и по возвратам один обрыв выглядел двумя.

    Мастер до конца здесь не идёт - остальные его шаги проверяют W1 и A10, - но
    устройство обязано уйти в сеть и прислать показания.
    """
    board = fresh_device.board
    ssid = stand.ap_ssid
    assert portal_mod.needs_setup(board), (
        'устройство знает сеть: это не первая настройка, сценарий не тот')

    started = find_line(stand, portal_mod.RE_AP_STARTED)
    assert started, 'нет строки о запуске точки портала'
    channel = int(started.group(1))
    portal_ssid = board.portal_ssid
    assert portal_ssid, 'имя точки портала неизвестно'

    with stand.router.channel(channel):
        стоял = stand.router.config().get('channel')
        assert стоял == str(channel), (
            f'роутер не встал на канал точки портала {channel}: {стоял}')

        # Список, снятый порталом до переезда роутера, показывает прежний канал:
        # ждём тот, на котором роутер стоит сейчас
        ours, seen = find_network(board, ssid, channel=channel)
        assert ours, (
            f'сети стенда {ssid} на канале {channel} нет в списке и после '
            f'повторных сканов: {[(net["ssid"], net["wifi_channel"]) for net in seen]}; '
            f'роутер: канал {stand.router.config().get("channel")}, '
            f'точка {stand.router.ap_enabled()}')

        answer = portal_mod.post_json(board, '/api/save_connect', ssid=ssid,
                                      password=cfg.ap_password,
                                      wifi_channel=ours['wifi_channel'],
                                      bssid=ours['bssid'], wizard='true')
        assert not answer.get('errors'), answer
        # error=0 - S_ANOTHER_CHANNEL (`active_point_api.cpp`, save_connect):
        # страница предупредит о потере связи. Каналы совпали - предупреждать не о чем
        assert 'error=0' not in answer.get('redirect', ''), (
            f'мастер предупредит о потере связи, хотя канал точки и роутера '
            f'один - {channel}: {answer}')

        было = board.outages
        dropped = start_connect(board)
        redirect, lost = wait_redirect(board, {CONNECTED})

        assert redirect == CONNECTED, (
            f'мастер не увидел подключения к сети: {redirect}\n{stand.log.tail()}')

        # Один обрыв виден тремя способами сразу - отказом запроса, возвратом
        # мастера и молчаливым возвратом транспорта, - поэтому складывать их
        # нельзя: 28.09 так вышло «два раза» на одном обрыве. Считаем эпизоды
        # транспорта (`atboard.py`, outages), а отказ запроса и возврат мастера
        # берут тот же эпизод, когда транспорту чинить уже нечего
        обрывов = board.outages - было or int(dropped or lost)
        assert обрывов == 1, (
            f'связь с порталом рвалась {обрывов} раз (запрос {dropped}, '
            f'мастер {lost}, эпизодов транспорта {board.outages - было}), а на '
            f'совпавшем канале обрыв ровно один - на подключении.\n'
            f'{stand.log.tail()}')

        # Точка осталась там же, где поднялась: вот чем W6 отличается от W5
        still, air = portal_in_air(board, portal_ssid, channel, STAY_S)
        assert still, (
            f'точки портала {portal_ssid} нет на канале {channel} после '
            f'подключения: в эфире {[(net.ssid, net.channel) for net in air]}')

        # Прошивка называет сеть и канал сама - последнее слово за ней
        connected = find_line(stand, RE_WIFI_CONNECTED, timeout=10)
        assert connected, f'прошивка не доложила о подключении\n{stand.log.tail()}'
        assert (connected.group(1), int(connected.group(2))) == (ssid, channel), (
            f'подключились не туда: {connected.group(0)}, ждали {ssid} на канале {channel}')

    # Возврат стенда - уже на его обычном канале. Внутри смены канала устройство
    # достаёт облако, но не приёмник стенда (прогон 28.09: cloud.waterius.ru
    # отвечает 200, 192.168.100.18:8010 - код -3), а это уже не про мастер
    fresh_device.leave()


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
