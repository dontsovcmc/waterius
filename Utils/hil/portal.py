"""
Портал настройки: обход всех страниц и файлов через AT-плату.

Ватериус в режиме настройки поднимает свою точку доступа и отдаёт с LittleFS
35 файлов. Проверяется не «страница открылась», а три разных утверждения, и
каждое ловит свою поломку:

* **файл доехал целиком** - образ LittleFS почти полон, и первым признаком
  переполнения будет обрезанный или отсутствующий файл, а не ошибка сборки;
* **статика совпадает байт в байт** с тем, что лежит в `ESP8266/data`, - это
  единственный способ заметить, что во флеш уехал старый образ: прошивка и
  файловая система шьются разными командами, и рассинхрон ничем больше не виден;
* **в html не осталось плейсхолдеров** - `%NAME%`, который прошивка не знает,
  остаётся в тексте как есть, и на странице его видит пользователь.

Байт в байт сверяются только статические файлы. Html идут через процессор
подстановки, поэтому их размер законно отличается от файла на диске, а сверять
их можно лишь с образом ровно той версии, что залита в устройство.
"""

from __future__ import annotations

import hashlib
import json
import re
import time
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator

from urllib.parse import urlencode

from loguru import logger

from .atboard import AtBoard, AtError, Response

HOST = '192.168.4.1'

# Строка прошивки о поднятой точке доступа. Имя точки содержит идентификатор
# чипа, поэтому в конфиге его не задать - только прочитать у устройства.
RE_AP_STARTED = re.compile(r'AP started on channel=(\d+) , ssid=(\S+)')

# Плейсхолдер шаблонизатора ESPAsyncWebServer. Регулярное выражение то же, что
# в тестах симулятора (simulator/test_simulator.js).
RE_PLACEHOLDER = re.compile(r'%[A-Za-z_][A-Za-z_0-9]*%')

# Страницы: адрес -> файл в образе. Зеркало обработчиков active_point.cpp.
# Одна страница отдаётся под несколькими адресами (страницы входов - с разными
# процессорами), поэтому таблица, а не список файлов.
PAGES: dict[str, str] = {
    '/': 'captive_portal_start.html',
    '/about.html': 'about.html',
    '/captive_portal.html': 'captive_portal.html',
    '/captive_portal_start.html': 'captive_portal_start.html',
    '/captive_portal_error.html': 'captive_portal_error.html',
    '/captive_portal_connected.html': 'captive_portal_connected.html',
    '/finish.html': 'finish.html',
    '/index.html': 'index.html',
    '/logs.html': 'logs.html',
    '/reset.html': 'reset.html',
    '/input/0/setup.html': 'input_setup.html',
    '/input/1/setup.html': 'input_setup.html',
    '/input/0/detect.html': 'input_detect.html',
    '/input/1/detect.html': 'input_detect.html',
    '/input/0/input_electro_detect.html': 'input_electro_detect.html',
    '/input/1/input_electro_detect.html': 'input_electro_detect.html',
    '/input/0/settings.html': 'input_settings.html',
    '/input/1/settings.html': 'input_settings.html',
    '/input/0/input_electro_settings.html': 'input_electro_settings.html',
    '/input/1/input_electro_settings.html': 'input_electro_settings.html',
    '/setup_send.html': 'setup_send.html',
    '/alarms.html': 'alarms.html',
    '/wifi_connect.html': 'wifi_connect.html',
    '/wifi_list.html': 'wifi_list.html',
    '/wifi_password.html': 'wifi_password.html',
    '/wifi_settings.html': 'wifi_settings.html',
}

# Обработчик есть, файла в образе нет. Проверено на живом устройстве: /start.html
# отвечает 404. Симулятор знает об этом же (simulator/test_simulator.js).
DEAD_ROUTES = ('/start.html',)

# Читаются без подстановки, поэтому сверяются с диском байт в байт.
FLAT_ASSETS = ('/favicon.ico', '/waterius_logs.txt')

# Страница появилась не сразу: на прошивке младше указанной её просто нет, и
# 404 там - верный ответ, а не поломка. Версии взяты из истории файлов образа.
PAGE_SINCE: dict[str, tuple[int, int, int]] = {
    '/alarms.html': (2, 0, 47),      # тревоги (#202)
}

# GET-эндпоинты, которые можно дёргать без последствий. /api/turnoff и
# /api/start_connect намеренно не здесь: первый выключает режим настройки,
# второй начинает подключение к домашней сети.
API_URLS = ('/api/main_status', '/api/status/0', '/api/status/1', '/api/networks')


class PortalError(Exception):
    pass


@dataclass
class Result:
    url: str
    status: int
    size: int
    expected: int | None = None      # размер файла в образе, если сверяемо
    same: bool | None = None         # совпал ли байт в байт
    placeholders: tuple[str, ...] = ()
    error: str = ''
    body: bytes = b''

    @property
    def ok(self) -> bool:
        return (not self.error and self.status == 200 and self.size > 0
                and self.same is not False and not self.placeholders)


def tree_version(root: Path) -> tuple[int, int, int] | None:
    """
    Версия прошивки в рабочем дереве - из platformio.ini.

    Нужна ровно для одного: сверять статику байт в байт можно лишь с образом
    той версии, что залита в устройство. Иначе тест ругался бы на каждое
    расхождение между веткой и прошивкой на столе.
    """
    text = (root / 'ESP8266' / 'platformio.ini').read_text(encoding='utf-8')
    m = re.search(r'firmware_version\s*=\s*.*?(\d+)\.(\d+)\.(\d+)', text)
    return (int(m.group(1)), int(m.group(2)), int(m.group(3))) if m else None


def _save(board: AtBoard, path: str, host: str, **params: str) -> dict:
    """
    Отправить форму портала и убедиться, что прошивка её приняла.

    Параметры уходят строкой запроса: `request->params()` не различает, откуда
    параметр приехал (`_parseReqHead` разбирает query у любого метода), а
    кодировать тело вторым способом ради того же результата незачем.

    Ответ - JSON; поле `errors` означает, что настройка не сохранена. Молча
    проглоченная ошибка здесь дороже всего: устройство останется в чужой сети,
    а тесты будут падать на пустом приёмнике.
    """
    answer = board.post(f'{path}?{urlencode(params)}', host)
    if answer.status != 200:
        raise PortalError(f'{path}: код {answer.status}')
    try:
        data = json.loads(answer.text)
    except ValueError as err:
        raise PortalError(f'{path}: ответ не JSON ({err}): {answer.text[:120]}')
    if data.get('errors'):
        raise PortalError(f'{path}: прошивка не приняла настройки: {data["errors"]}')
    return data


def save_wifi(board: AtBoard, ssid: str, password: str, host: str = HOST) -> None:
    """Сеть, к которой Ватериус будет подключаться в рабочем режиме."""
    _save(board, '/api/save_connect', host, ssid=ssid, password=password)


def save_server(board: AtBoard, url: str, host: str = HOST) -> None:
    """
    Свой сервер - приёмник стенда.

    `http_on` обязателен в том же запросе: `http_url` прошивка сохраняет только
    при включённом флаге (`active_point_api.cpp`, `applySettings` сначала
    проходит по галочкам, потом по остальным полям).
    """
    _save(board, '/api/save', host, http_on='1', http_url=url)


def turnoff(board: AtBoard, host: str = HOST) -> None:
    """
    Выйти из режима настройки.

    Подключение к сети портал умеет и сам (`/api/start_connect`), но при смене
    сети ЕСП уводит свою точку на канал новой - и AT-плата теряет ту самую
    точку, через которую мы наблюдаем. Поэтому подключение оставляем обычному
    пробуждению: после `turnoff` прошивка перезапускается и выходит на связь
    уже по новым настройкам.
    """
    try:
        board.get('/api/turnoff', host)
    except AtError as err:
        logger.warning(f'портал не ответил на turnoff: {err}')


def configure(board: AtBoard, ssid: str, password: str, url: str,
              host: str = HOST) -> None:
    """Весь этап настройки: сеть, сервер, выход из портала."""
    logger.info(f'портал: сеть {ssid}, сервер {url}')
    save_wifi(board, ssid, password, host)
    save_server(board, url, host)
    turnoff(board, host)


def asset_urls(data_dir: Path) -> dict[str, Path]:
    """Статика образа: адрес -> файл. Список берётся с диска, а не из кода."""
    urls: dict[str, Path] = {}
    for folder in ('static', 'images'):
        for path in sorted((data_dir / folder).glob('*')):
            urls[f'/{folder}/{path.name}'] = path
    for url in FLAT_ASSETS:
        path = data_dir / url.lstrip('/')
        if path.exists():
            urls[url] = path
    return urls


def find_ap(lines: list[str]) -> str | None:
    """Имя точки доступа портала из лога устройства."""
    for line in reversed(lines):
        m = RE_AP_STARTED.search(line)
        if m:
            return m.group(2)
    return None


@contextmanager
def session(cfg: Any, stand: Any, timeout: float = 90.0) -> Iterator[AtBoard]:
    """
    Ватериус в режиме настройки, AT-плата в его сети.

    Выход - командой `/api/turnoff`, а не по таймауту: иначе следующий тест
    ждал бы десять минут сторожевого таймера портала. Пока устройство не
    уснуло, ЕСП запитана и нажатие кнопки до attiny не доходит, поэтому
    выходим только дождавшись сна.
    """
    stand.log.clear()
    stand.dut.hold_button()

    ssid = None
    deadline = time.time() + timeout
    while time.time() < deadline:
        stand.log.poll()
        ssid = find_ap(stand.log.lines)
        if ssid:
            break
        time.sleep(1)
    if not ssid:
        raise PortalError('точка доступа портала не поднялась')
    logger.info(f'портал: {ssid}')

    device = AtBoard(cfg.atboard_port)
    logger.info(f'адрес AT-платы: {device.join(ssid)}')
    try:
        yield device
    finally:
        try:
            device.get('/api/turnoff', HOST)
        except Exception as err:
            logger.warning(f'портал не закрылся командой: {err}')
        device.close()
        stand.wait_asleep()


def unresolved(text: str) -> tuple[str, ...]:
    return tuple(dict.fromkeys(RE_PLACEHOLDER.findall(text)))


def _fetch(board: AtBoard, url: str, host: str) -> Response | str:
    try:
        return board.get(url, host)
    except AtError as err:
        return str(err)


def check_pages(board: AtBoard, host: str = HOST) -> list[Result]:
    results = []
    for url in PAGES:
        answer = _fetch(board, url, host)
        if isinstance(answer, str):
            results.append(Result(url, 0, 0, error=answer))
            continue
        results.append(Result(url, answer.status, len(answer.body),
                              placeholders=unresolved(answer.text),
                              body=answer.body))
    return results


def check_assets(board: AtBoard, data_dir: Path, host: str = HOST) -> list[Result]:
    results = []
    for url, path in asset_urls(data_dir).items():
        answer = _fetch(board, url, host)
        if isinstance(answer, str):
            results.append(Result(url, 0, 0, error=answer))
            continue
        want = path.read_bytes()
        same = (hashlib.sha256(want).digest()
                == hashlib.sha256(answer.body).digest())
        results.append(Result(url, answer.status, len(answer.body),
                              expected=len(want), same=same, body=answer.body))
    return results


def check_api(board: AtBoard, host: str = HOST) -> list[Result]:
    results = []
    for url in API_URLS:
        answer = _fetch(board, url, host)
        if isinstance(answer, str):
            results.append(Result(url, 0, 0, error=answer))
            continue
        results.append(Result(url, answer.status, len(answer.body),
                              body=answer.body))
    return results


if __name__ == '__main__':
    import argparse
    import sys
    import time

    from loguru import logger
    from metf_python_client import METFClient

    from .config import load
    from .dut import Dut
    from .logwatch import LogWatcher

    parser = argparse.ArgumentParser(description='обход портала через AT-плату')
    parser.add_argument('--port', default='/dev/cu.usbserial-0001',
                        help='последовательный порт AT-платы')
    parser.add_argument('--ssid', help='имя точки портала; по умолчанию - из лога')
    parser.add_argument('--press', action='store_true',
                        help='сначала ввести устройство в режим настройки кнопкой')
    parser.add_argument('--data', default=None, help='каталог образа ESP8266/data')
    parser.add_argument('--config', default=None, help='путь к stand.ini')
    args = parser.parse_args()

    cfg = load(args.config)
    data_dir = Path(args.data) if args.data else \
        Path(__file__).resolve().parents[2] / 'ESP8266' / 'data'

    ssid = args.ssid
    if args.press or not ssid:
        api = METFClient(cfg.metf_host)
        dut = Dut(api, cfg.button_pin, cfg.ch0_pin, cfg.ch1_pin, cfg.reset_pin)
        log = LogWatcher(api)
        if args.press:
            dut.init()
            log.clear()
            logger.info('удерживаю кнопку - режим настройки')
            dut.hold_button()
        deadline = time.time() + 90
        while time.time() < deadline and not ssid:
            log.poll()
            ssid = find_ap(log.lines)
            if not ssid:
                time.sleep(1)
        if not ssid:
            logger.error('точка портала не поднялась')
            sys.exit(1)
    logger.info(f'точка портала: {ssid}')

    board = AtBoard(args.port)
    logger.info(f'адрес платы: {board.join(ssid)}')

    results = check_pages(board) + check_assets(board, data_dir) + check_api(board)
    bad = 0
    for r in results:
        mark = 'ok ' if r.ok else 'ПЛОХО'
        note = ''
        if r.expected is not None:
            note = f' образ {r.expected}' + ('' if r.same else ' РАЗОШЁЛСЯ')
        if r.placeholders:
            note += ' плейсхолдеры: ' + ','.join(r.placeholders)
        if r.error:
            note += ' ' + r.error
        bad += not r.ok
        print(f'{mark:5} {r.url:40} {r.status} {r.size:>6}{note}')
    print(f'\nвсего {len(results)}, плохих {bad}')
