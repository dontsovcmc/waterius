"""
Эфир вокруг стенда: на чём вещает точка и кто рядом.

Улики про эфир нужны до первого теста, а не после разбора трёх часов лога:
оборванные тела посылок и провалы чтения лога рождаются здесь, а выглядят как
дефект прошивки (05_air-and-loss.md).

Смотрим глазами Мака - он и так участник: на нём приёмник, брокер и управление
METF. `system_profiler` перечисляет и свою сеть, и соседские, называя у каждой
канал, ширину полосы и сигнал. Это единственный источник про **полосу**: у
роутера стенда такой команды в консоли нет, `show config` печатает только
канал.
"""

from __future__ import annotations

import re
import subprocess
import time
from dataclasses import dataclass

from loguru import logger

SCAN_CMD = ('system_profiler', 'SPAirPortDataType')
SCAN_TIMEOUT = 25.0
# Мак отдаёт список прошлого скана: только что поднятая точка появляется в нём
# не сразу.
SCAN_TRIES = 3
SCAN_PAUSE = 4.0

# Полосы 2,4 ГГц шириной 20 МГц стоят через 5 МГц: не перекрываются только
# каналы, разнесённые на пять номеров.
CHANNEL_GAP = 5

_SSID = re.compile(r'^\s{10,}(\S.*?):\s*$')
_CHANNEL = re.compile(r'^\s+Channel:\s+(\d+)\s+\(([\d.]+)GHz,\s*(\d+)MHz\)')
_SIGNAL = re.compile(r'^\s+Signal / Noise:\s+(-?\d+)\s+dBm')


@dataclass(frozen=True)
class Network:
    """Сеть, какой её слышит Мак."""

    ssid: str
    channel: int
    width: int          # ширина полосы, МГц
    rssi: int | None    # у соседей бывает пусто


def scan(want: str = '') -> list[Network]:
    """
    Перечислить сети вокруг. Пусто - если сканировать нечем (не macOS).

    `want` - имя сети, ради которой всё и затевалось: Мак отдаёт список
    прошлого скана, и сразу после перезапуска точки её там ещё нет. Тогда
    список берётся заново, но не бесконечно: скан - улика, а не условие работы,
    и отказ сканера прогон не роняет.
    """
    for last in range(SCAN_TRIES - 1, -1, -1):
        found = _scan_once()
        if not want or not found or any(n.ssid == want for n in found) or not last:
            return found
        time.sleep(SCAN_PAUSE)
    return []


def _scan_once() -> list[Network]:
    try:
        out = subprocess.run(SCAN_CMD, capture_output=True, text=True,
                             timeout=SCAN_TIMEOUT).stdout
    except (OSError, subprocess.SubprocessError) as err:
        logger.warning(f'эфир: скан не вышел ({err}) - улик про полосу не будет')
        return []

    found: list[Network] = []
    ssid = ''
    channel = width = 0
    rssi: int | None = None

    def flush() -> None:
        if ssid and channel:
            found.append(Network(ssid, channel, width, rssi))

    for line in out.splitlines():
        name = _SSID.match(line)
        if name and not name.group(1).endswith(('Information', 'Networks')):
            flush()
            ssid, channel, width, rssi = name.group(1), 0, 0, None
            continue
        where = _CHANNEL.match(line)
        if where:
            channel, width = int(where.group(1)), int(where.group(3))
            continue
        signal = _SIGNAL.match(line)
        if signal:
            rssi = int(signal.group(1))
    flush()
    return found


def check(ap_ssid: str, channel: int, want_width: int) -> list[Network]:
    """
    Сверить точку стенда с тем, что слышно в эфире, и назвать беды.

    Беды именно называются, а не роняют прогон: чинятся они руками (полоса -
    прошивкой роутера, соседи - выбором канала), а тесты гонять надо и сегодня.
    """
    seen = scan(ap_ssid)
    if not seen:
        return seen

    ours = next((n for n in seen if n.ssid == ap_ssid), None)
    if ours is None:
        logger.warning(f'эфир: точку стенда «{ap_ssid}» в скане не видно - '
                       f'проверить полосу и соседей нечем')
        return seen

    if ours.channel != channel:
        logger.warning(f'эфир: роутер говорит про канал {channel}, а в эфире '
                       f'точка на {ours.channel} - настройка не применилась')
    if want_width and ours.width > want_width:
        logger.warning(
            f'эфир: точка стенда вещает полосой {ours.width} МГц, а нужно '
            f'{want_width}. Широкая полоса занимает пять каналов вместо одного '
            f'и мешает соседям, а ESP8266 шире {want_width} МГц всё равно не '
            f'умеет. В консоли роутера команды на полосу нет: лечится сборкой '
            f'его прошивки с esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20)')

    close = [n for n in seen
             if n.ssid != ap_ssid and abs(n.channel - ours.channel) < CHANNEL_GAP
             and n.channel < 15]                  # 5 ГГц нам не мешают
    if close:
        who = ', '.join(f'{n.ssid} (канал {n.channel}'
                        + (f', {n.rssi} дБм)' if n.rssi is not None else ')')
                        for n in sorted(close, key=lambda n: n.channel))
        logger.info(f'эфир: точка стенда на канале {ours.channel}, {ours.width} МГц; '
                    f'в её полосе сидят: {who}')
    return seen
