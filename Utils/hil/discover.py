"""
Поиск плат стенда, когда адрес из `stand.ini` устарел.

Адреса раздаёт DHCP домашнего роутера, и они переезжают сами: 29 сентября
после перезагрузки роутера METF, точка стенда и Мак получили новые, а в файле
остались вчерашние. Стенд от этого не жалуется - он молчит по таймауту и
падает на первой же фикстуре, не назвав причины.

Ищем в три хода, от дешёвого к дорогому:

1. записанный адрес - если отвечает, никакого поиска не нужно;
2. имя в mDNS - `metf.local` (прошивка METF с пятнадцатого протокола),
   `esp32-nat-router.local` (прошивка точки стенда);
3. обход своей сети - последнее средство, когда имя не разошлось или плата
   старее.

Опознаём каждую плату её собственным ответом, а не просто открытым портом:
в домашней сети живут чужие устройства, и «кто-то ответил по 80» - не улика.
"""

from __future__ import annotations

import socket
from collections.abc import Callable, Iterator
from concurrent.futures import ThreadPoolExecutor

import requests
from loguru import logger

# Плата отвечает за доли секунды; секунды здесь - это уже чужое устройство,
# которое молчит, а обход ждёт их все разом
PROBE_S = 1.0
# Своя сеть - /24: столько адресов обходятся за пару секунд
WORKERS = 64
# Консоль точки стенда (router.py: CONSOLE_PORT)
CONSOLE_PORT = 2323


def metf_answers(host: str) -> bool:
    """
    METF по `GET /version`: она отвечает голым числом протокола.

    Не `/wifi`: он есть только с девятого протокола и отдаёт объект, в котором
    легко узнать чужой JSON, а `/version` короток и есть у любой прошивки METF.
    """
    try:
        answer = requests.get(f'http://{host}/version', timeout=PROBE_S)
    except (requests.RequestException, OSError):
        return False
    return answer.ok and answer.text.strip().isdigit()


def router_answers(host: str) -> bool:
    """Точка стенда - по своей консоли: порт 2323 в домашней сети больше ничей."""
    try:
        with socket.create_connection((host, CONSOLE_PORT), timeout=PROBE_S):
            return True
    except OSError:
        return False


# Плата: как зовётся в mDNS и чем себя выдаёт
BOARDS = {
    'metf': ('metf.local', metf_answers),
    'router': ('esp32-nat-router.local', router_answers),
}


def by_name(name: str) -> str:
    """Адрес за именем mDNS; пусто - имя не разошлось."""
    try:
        return socket.gethostbyname(name)
    except OSError:
        return ''


def neighbours(near: str) -> Iterator[str]:
    """Адреса сети /24, в которой стоит `near`; сам `near` пропускаем."""
    head = near.rsplit('.', 1)[0]
    for last in range(1, 255):
        host = f'{head}.{last}'
        if host != near:
            yield host


def sweep(near: str, answers: Callable[[str], bool]) -> str:
    """Первый сосед, который отозвался как нужная плата; пусто - никто."""
    hosts = list(neighbours(near))
    with ThreadPoolExecutor(max_workers=WORKERS) as pool:
        for host, found in zip(hosts, pool.map(answers, hosts), strict=True):
            if found:
                return host
    return ''


def find(kind: str, written: str, near: str) -> str:
    """
    Адрес платы `kind`. Пусто - не нашлась, и пусть отказ объяснит тот, кому
    она нужна: у него есть что сказать про саму плату.
    """
    name, answers = BOARDS[kind]

    if written and answers(written):
        return written

    at_name = by_name(name)
    if at_name and answers(at_name):
        logger.warning(f'{kind}: {written or "адрес не задан"} не отвечает, '
                       f'но {name} - это {at_name}')
        return at_name

    found = sweep(near, answers)
    if found:
        logger.warning(f'{kind}: {written or "адрес не задан"} не отвечает, '
                       f'нашлась обходом сети на {found}')
    return found
