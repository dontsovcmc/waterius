"""
Управление лабораторной точкой доступа: WT32-ETH01 с прошивкой esp32_nat_router.

Зачем это в стенде. Половина сценариев ручного плана (docs/manual-test-plan.md)
начинается словами "выключите роутер" - и ровно это не давало прогонять тесты без
человека. Плата даёт три вещи, которых нет ни у домашнего роутера, ни у телефона:

1. Точка доступа гасится и поднимается одной командой, без перезагрузки.
2. Канал точки задаётся руками. Это возможно только на сборке с проводным
   аплинком: при аплинке по Wi-Fi радио одно, и точка обязана жить на канале
   роутера. У WT32-ETH01 аплинк - витая пара, поэтому канал наш.
3. Правила фильтра режут трафик выборочно. "Сети нет" - это ap disable, а вот
   "сеть есть, а сервер недоступен" и "облако молчит, брокер отвечает" (ради чего
   заводилась маска квитанции) не получить ничем другим.

Транспорт по умолчанию - USB-serial. Команды ap/set_ap рвут Wi-Fi, а сетевая
консоль живёт в той же сети; по проводу канал управления от этого не зависит.

Утилита для рук:

    python router.py --port /dev/cu.usbserial-XXXX ap off
    python router.py --port /dev/cu.usbserial-XXXX channel 11
    python router.py --port /dev/cu.usbserial-XXXX raw "show config"
"""

from __future__ import annotations

import argparse
import re
import socket
import threading
import time
from contextlib import contextmanager
from dataclasses import dataclass, field
from typing import Iterator, Protocol

import serial
from loguru import logger

ROUTER_BAUD = 115200
CONSOLE_PORT = 2323
PCAP_PORT = 19000

# Прошивка отвечает по-разному в зависимости от команды, поэтому конца ответа
# ждём по тишине в эфире, а не по приглашению.
IDLE_GAP = 0.25
CMD_TIMEOUT = 4.0

# Смена канала и SSID требуют перезагрузки: точка пропадает на эти секунды.
RESTART_WAIT = 12.0

ERROR_MARKERS = (
    'Unrecognized command',
    'Command returned non-zero',
    'Invalid arguments',
)


class RouterError(RuntimeError):
    """Роутер не принял команду или ответил ошибкой."""


class Transport(Protocol):
    def write_line(self, line: str) -> None: ...

    def read_idle(self, timeout: float, idle: float) -> str: ...

    def drain(self) -> None: ...

    def close(self) -> None: ...


class SerialTransport:
    """Консоль по USB-TTL. У WT32-ETH01 своего USB нет - нужен переходник."""

    def __init__(self, port: str, baud: int = ROUTER_BAUD) -> None:
        self._ser = serial.Serial(port, baud, timeout=0.1)

    def write_line(self, line: str) -> None:
        self._ser.write(line.encode() + b'\r\n')
        self._ser.flush()

    def read_idle(self, timeout: float, idle: float) -> str:
        deadline = time.time() + timeout
        chunks: list[bytes] = []
        last = time.time()
        while time.time() < deadline:
            data = self._ser.read(4096)
            if data:
                chunks.append(data)
                last = time.time()
            elif chunks and time.time() - last >= idle:
                break
            else:
                time.sleep(0.02)
        return b''.join(chunks).decode(errors='replace')

    def drain(self) -> None:
        self._ser.reset_input_buffer()

    def close(self) -> None:
        self._ser.close()


class TcpTransport:
    """
    Сетевая консоль (remote_console). Удобна, когда плата стоит далеко, но
    командой ap disable вы рискуете отрезать сами себя, если консоль привязана
    к точке доступа. Пароль тот же, что у веб-интерфейса, спрашивается сразу
    после подключения.
    """

    def __init__(self, host: str, password: str, port: int = CONSOLE_PORT) -> None:
        self._sock = socket.create_connection((host, port), timeout=5.0)
        self._sock.settimeout(0.2)
        time.sleep(0.3)
        self._sock.sendall(password.encode() + b'\r\n')
        time.sleep(0.3)
        self.drain()

    def write_line(self, line: str) -> None:
        self._sock.sendall(line.encode() + b'\r\n')

    def read_idle(self, timeout: float, idle: float) -> str:
        deadline = time.time() + timeout
        chunks: list[bytes] = []
        last = time.time()
        while time.time() < deadline:
            try:
                data = self._sock.recv(4096)
            except socket.timeout:
                data = b''
            if data:
                chunks.append(data)
                last = time.time()
            elif chunks and time.time() - last >= idle:
                break
        return b''.join(chunks).decode(errors='replace')

    def drain(self) -> None:
        try:
            while self._sock.recv(4096):
                pass
        except socket.timeout:
            pass

    def close(self) -> None:
        self._sock.close()


@dataclass
class RouterState:
    """Снимок настроек, к которому фикстура возвращает роутер после теста."""

    config: dict[str, str] = field(default_factory=dict)
    acl_rules: list[str] = field(default_factory=list)


class NatRouter:
    """
    Обёртка над CLI. Состояние всегда читаем у роутера (show), а не помним у
    себя: разъехавшийся стенд иначе даёт зелёный тест на неверных настройках.
    """

    def __init__(self, transport: Transport) -> None:
        self._t = transport

    # --- основа ----------------------------------------------------------

    def cmd(self, line: str, timeout: float = CMD_TIMEOUT) -> str:
        """Отправить команду и вернуть ответ без эха самой команды."""
        self._t.drain()
        self._t.write_line(line)
        out = self._t.read_idle(timeout, IDLE_GAP)

        for marker in ERROR_MARKERS:
            if marker in out:
                raise RouterError(f'{line!r}: {marker}\n{out.strip()}')

        return _strip_echo(out, line)

    def version(self) -> str:
        return self.cmd('version').strip()

    def show(self, section: str = 'config') -> str:
        return self.cmd(f'show {section}', timeout=6.0)

    def config(self) -> dict[str, str]:
        """`show config` в словарь. Ключи - как их печатает прошивка."""
        return _parse_kv(self.show('config'))

    def status(self) -> dict[str, str]:
        return _parse_kv(self.show('status'))

    def clients(self) -> list[dict[str, str]]:
        """Подключённые к точке клиенты: mac, ip, имя (если известно)."""
        out = self.show('status')
        found = []
        for line in out.splitlines():
            m = re.search(
                r'((?:[0-9a-f]{2}:){5}[0-9a-f]{2})\s+(\d+\.\d+\.\d+\.\d+)\s*(\S*)',
                line, re.IGNORECASE)
            if m:
                found.append({'mac': m.group(1).lower(), 'ip': m.group(2), 'name': m.group(3)})
        return found

    def restart(self, wait: float = RESTART_WAIT) -> None:
        """
        Перезагрузка. После неё в порт летит мусор бутлоадера, поэтому ждём
        и ресинхронизируемся отдельной командой - иначе ответ на следующую
        команду приедет вперемешку с баннером.
        """
        self._t.write_line('restart')
        time.sleep(wait)
        self._t.drain()
        self.version()

    # --- точка доступа ---------------------------------------------------

    def ap(self, enabled: bool) -> None:
        """Единственная команда, которая действует сразу, без перезагрузки."""
        self.cmd('ap enable' if enabled else 'ap disable')

    def set_ap(self, ssid: str, password: str) -> None:
        self.cmd(f'set_ap {ssid} {password}')

    def set_ap_channel(self, channel: int) -> None:
        """0 - авто, 1..13 - фиксированный. Только сборка с аплинком Ethernet."""
        self.cmd(f'set_ap_channel {channel}')

    def set_tx_power(self, dbm: int) -> None:
        self.cmd(f'set_tx_power {dbm}')

    def dhcp_reserve(self, mac: str, ip: str, name: str = '') -> None:
        """
        Закрепить адрес за устройством. Без этого правила фильтра пришлось бы
        переписывать после каждой выдачи адреса.
        """
        suffix = f' -- {name}' if name else ''
        self.cmd(f'dhcp_reserve add {mac} {ip}{suffix}')

    def client_stats(self, enabled: bool = True) -> None:
        self.cmd(f'client_stats {"enable" if enabled else "disable"}')

    # --- фильтр ----------------------------------------------------------

    def acl_add(self, rule: str) -> None:
        self.cmd(f'acl add {rule}')

    def acl_clear(self) -> None:
        """
        Снять все правила. Прошивка удаляет по тому же описанию, которым
        правило заводилось, поэтому список берём из show acl.
        """
        for rule in self.acl_rules():
            try:
                self.cmd(f'acl del {rule}')
            except RouterError:
                logger.warning(f'правило не снялось: {rule}')

    def acl_rules(self) -> list[str]:
        out = self.show('acl')
        rules = []
        for line in out.splitlines():
            line = line.strip()
            if line.startswith(('to_esp', 'from_esp', 'to_ap', 'from_ap')):
                rules.append(line)
        return rules

    # --- сценарии --------------------------------------------------------

    def block_all(self, ip: str) -> None:
        """Интернета нет, Wi-Fi живёт: тесты G4, F4, D2b."""
        self.acl_add(f'from_ap IP {ip} * any * deny')

    def block_port(self, ip: str, port: int) -> None:
        """Один получатель недоступен, остальные живы: F2, F3, G5."""
        self.acl_add(f'from_ap TCP {ip} * any {port} deny')

    @contextmanager
    def ap_off(self) -> Iterator[None]:
        self.ap(False)
        try:
            yield
        finally:
            self.ap(True)

    @contextmanager
    def blocked(self, ip: str, port: int | None = None) -> Iterator[None]:
        if port is None:
            self.block_all(ip)
        else:
            self.block_port(ip, port)
        try:
            yield
        finally:
            self.acl_clear()

    @contextmanager
    def channel(self, number: int) -> Iterator[None]:
        was = self.config().get('ap_channel', '0')
        self.set_ap_channel(number)
        self.restart()
        try:
            yield
        finally:
            self.set_ap_channel(int(was))
            self.restart()

    @contextmanager
    def ssid(self, name: str, password: str) -> Iterator[None]:
        cfg = self.config()
        was_ssid = cfg.get('ap_ssid', '')
        was_pass = cfg.get('ap_password', '')
        self.set_ap(name, password)
        self.restart()
        try:
            yield
        finally:
            self.set_ap(was_ssid, was_pass)
            self.restart()

    # --- ожидание клиента ------------------------------------------------

    def wait_client(self, mac: str, timeout: float = 120.0) -> bool:
        """
        Дождаться, когда Ватериус подключится. Это синхронизация вместо
        sleep: ЕСП живёт секунды, и угадывать её пробуждение по таймеру -
        главный источник мигающих тестов.
        """
        mac = mac.lower()
        deadline = time.time() + timeout
        while time.time() < deadline:
            if any(c['mac'] == mac for c in self.clients()):
                return True
            time.sleep(1.0)
        return False

    # --- снимок и возврат ------------------------------------------------

    def snapshot(self) -> RouterState:
        return RouterState(config=self.config(), acl_rules=self.acl_rules())

    def restore(self, state: RouterState) -> None:
        """
        Вернуть роутер в исходное состояние. Обязателен в teardown: тест,
        упавший с выключенной точкой доступа, иначе уронит весь прогон -
        Ватериус просто не найдёт сеть.
        """
        self.acl_clear()
        self.ap(True)

        now = self.config()
        need_restart = False

        was_ssid = state.config.get('ap_ssid')
        was_pass = state.config.get('ap_password')
        if was_ssid and now.get('ap_ssid') != was_ssid:
            self.set_ap(was_ssid, was_pass or '')
            need_restart = True

        was_channel = state.config.get('ap_channel')
        if was_channel and now.get('ap_channel') != was_channel:
            self.set_ap_channel(int(was_channel))
            need_restart = True

        if need_restart:
            self.restart()

    # --- дамп трафика ----------------------------------------------------

    @contextmanager
    def capture(self, host: str, path: str) -> Iterator[None]:
        """
        Дамп трафика точки доступа в файл. Прикладывается к упавшему тесту:
        по логу видно, что прошивка думала, а по дампу - что ушло в эфир.
        Буферизация включается только пока кто-то подключён к порту.
        """
        self.cmd('pcap mode promisc')
        stop = threading.Event()

        def pump() -> None:
            try:
                with socket.create_connection((host, PCAP_PORT), timeout=5.0) as s, \
                        open(path, 'wb') as f:
                    s.settimeout(1.0)
                    while not stop.is_set():
                        try:
                            data = s.recv(8192)
                        except socket.timeout:
                            continue
                        if not data:
                            break
                        f.write(data)
            except OSError as err:
                logger.warning(f'дамп трафика не собран: {err}')

        thread = threading.Thread(target=pump, daemon=True)
        thread.start()
        try:
            yield
        finally:
            stop.set()
            thread.join(timeout=3.0)
            self.cmd('pcap mode off')

    def close(self) -> None:
        self._t.close()


def _strip_echo(out: str, line: str) -> str:
    """Консоль повторяет введённую команду - убираем, чтобы не мешала разбору."""
    lines = out.splitlines()
    if lines and lines[0].strip().endswith(line.strip()):
        lines = lines[1:]
    return '\n'.join(lines).strip()


def _parse_kv(text: str) -> dict[str, str]:
    """
    `show` печатает пары вида "ключ: значение" и "ключ = значение".
    Разбираем оба, ключ приводим к snake_case - по нему потом сверяем снимок.
    """
    result: dict[str, str] = {}
    for line in text.splitlines():
        m = re.match(r'\s*([A-Za-z][A-Za-z0-9 _.-]*?)\s*[:=]\s*(.*?)\s*$', line)
        if not m:
            continue
        key = re.sub(r'[ .-]+', '_', m.group(1).strip()).lower()
        result[key] = m.group(2)
    return result


def connect(port: str | None = None, host: str | None = None,
            password: str = '') -> NatRouter:
    """Собрать роутер поверх порта или поверх сетевой консоли."""
    if port:
        return NatRouter(SerialTransport(port))
    if host:
        return NatRouter(TcpTransport(host, password))
    raise ValueError('нужен --port или --host')


def main() -> None:
    parser = argparse.ArgumentParser(description='Лабораторная точка доступа Ватериуса')
    parser.add_argument('--port', help='последовательный порт, например /dev/cu.usbserial-XXXX')
    parser.add_argument('--host', help='адрес сетевой консоли вместо порта')
    parser.add_argument('--password', default='', help='пароль сетевой консоли')

    sub = parser.add_subparsers(dest='action', required=True)
    p_ap = sub.add_parser('ap', help='включить или выключить точку доступа')
    p_ap.add_argument('state', choices=['on', 'off'])
    p_ch = sub.add_parser('channel', help='канал точки доступа (0 - авто)')
    p_ch.add_argument('number', type=int)
    p_ssid = sub.add_parser('ssid', help='переименовать точку')
    p_ssid.add_argument('name')
    p_ssid.add_argument('password')
    p_block = sub.add_parser('block', help='зарезать трафик клиента')
    p_block.add_argument('ip')
    p_block.add_argument('--port', dest='dst_port', type=int, default=None)
    sub.add_parser('unblock', help='снять все правила фильтра')
    p_show = sub.add_parser('show', help='показать состояние')
    p_show.add_argument('section', nargs='?', default='config')
    p_raw = sub.add_parser('raw', help='отправить команду как есть')
    p_raw.add_argument('line')

    args = parser.parse_args()
    router = connect(args.port, args.host, args.password)
    try:
        if args.action == 'ap':
            router.ap(args.state == 'on')
        elif args.action == 'channel':
            router.set_ap_channel(args.number)
            router.restart()
        elif args.action == 'ssid':
            router.set_ap(args.name, args.password)
            router.restart()
        elif args.action == 'block':
            if args.dst_port:
                router.block_port(args.ip, args.dst_port)
            else:
                router.block_all(args.ip)
        elif args.action == 'unblock':
            router.acl_clear()
        elif args.action == 'show':
            print(router.show(args.section))
        elif args.action == 'raw':
            print(router.cmd(args.line))
    finally:
        router.close()


if __name__ == '__main__':
    main()
