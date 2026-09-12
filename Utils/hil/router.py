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
from typing import Callable, Iterator, Protocol

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

# Списки фильтра, как их называет прошивка (`acl` без аргументов)
ACL_LISTS = ('from_esp', 'to_esp', 'from_ap', 'to_ap')

ERROR_MARKERS = (
    'Unrecognized command',
    'Command returned non-zero',
    'Invalid arguments',
    # Разбор аргументов ругается своими словами, и без этих двух строк ошибка
    # проглатывалась: команда не выполнена, а тест считал, что всё хорошо
    'missing option',
    'excess option',
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
        self._host = host
        self._password = password
        self._port = port
        self._connect()

    def _connect(self) -> None:
        self._sock = socket.create_connection((self._host, self._port), timeout=5.0)
        self._sock.settimeout(0.2)
        time.sleep(0.3)
        self._sock.sendall(self._password.encode() + b'\r\n')
        time.sleep(0.3)
        self.drain()

    def reconnect(self, timeout: float = 60.0) -> None:
        """
        Поднять консоль заново после перезагрузки платы.

        Перезагрузка рвёт сокет, и это не ошибка: на проводе тот же restart
        оставляет порт живым, а по сети консоль умирает вместе с платой.
        """
        try:
            self._sock.close()
        except OSError:
            pass
        deadline = time.time() + timeout
        while True:
            try:
                self._connect()
                return
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(2.0)

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
        except (socket.timeout, OSError):
            pass

    def close(self) -> None:
        self._sock.close()


@dataclass
class RouterState:
    """Снимок настроек, к которому фикстура возвращает роутер после теста."""

    config: dict[str, str] = field(default_factory=dict)
    acl_rules: list[str] = field(default_factory=list)

    # Имена ключей - те, что печатает `show config` (см. _parse_kv). Раньше
    # снимок читался по выдуманным `ap_ssid`/`ap_channel`, и восстановление
    # молча не срабатывало ни разу.
    @property
    def ssid(self) -> str:
        return self.config.get('ssid', '')

    @property
    def channel(self) -> str:
        return self.config.get('channel', '')


class NatRouter:
    """
    Обёртка над CLI. Состояние всегда читаем у роутера (show), а не помним у
    себя: разъехавшийся стенд иначе даёт зелёный тест на неверных настройках.
    """

    def __init__(self, transport: Transport, ap_password: str = '') -> None:
        self._t = transport
        # Пароль точки доступа неоткуда прочитать: `show config` печатает
        # звёздочки. Без него нельзя вернуть имя точки после теста, который
        # его менял, - поэтому он приходит из stand.ini.
        self.ap_password = ap_password

    def _ap_password(self) -> str:
        assert self.ap_password, (
            'нужен [router] ap_password в stand.ini: имя точки восстанавливается '
            'вместе с паролем, а прочитать его у платы нельзя - show config '
            'печатает звёздочки')
        return self.ap_password

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

    def cmd_checked(self, line: str, applied: Callable[[], bool],
                    what: str) -> None:
        """
        Выполнить команду и убедиться по состоянию роутера, что она подействовала.

        Ответ прошивки - свободный текст, и судить по нему об успехе значит
        держать список всех её формулировок. Один такой промах уже был:
        `dhcp_reserve` с именем устройства отвечал `excess option`, в список не
        попадал, и стенд считал адрес закреплённым. Состояние врать не умеет.
        """
        self.cmd(line)
        if not applied():
            raise RouterError(f'{what}: команда выполнена, а состояние не изменилось\n{line}')

    def config(self) -> dict[str, str]:
        """`show config` в словарь. Ключи - как их печатает прошивка."""
        return _parse_kv(self.show('config'))

    def nat_enabled(self) -> bool | None:
        """
        Транслирует ли точка трафик наружу.

        Отдельная проверка нужна потому, что снаружи это выглядит как исправная
        сеть: клиент получает адрес, видит саму точку - и не видит больше
        ничего. `show config` про NAT молчит, состояние печатает только
        `set_ap_nat` без аргументов.
        """
        out = self.cmd('set_ap_nat')
        if 'NAT: enabled' in out:
            return True
        if 'NAT: disabled' in out:
            return False
        return None

    def set_nat(self, enabled: bool) -> None:
        """Включить или выключить трансляцию. Применяется только после restart."""
        self.cmd(f'set_ap_nat {"on" if enabled else "off"}')

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
        try:
            self._t.write_line('restart')
        except OSError:
            pass                       # плата успела уйти в перезагрузку
        time.sleep(wait)
        reconnect = getattr(self._t, 'reconnect', None)
        if reconnect is not None:
            reconnect()                # сетевая консоль умирает вместе с платой
        else:
            self._t.drain()
        self.version()

    # --- точка доступа ---------------------------------------------------

    def ap(self, enabled: bool) -> None:
        """Единственная команда, которая действует сразу, без перезагрузки."""
        self.cmd_checked(
            'ap enable' if enabled else 'ap disable',
            lambda: self.ap_enabled() is enabled,
            'точка доступа')

    def ap_enabled(self) -> bool | None:
        """Состояние точки по `show status`. None - строки в ответе нет."""
        m = re.search(r'AP interface:\s*(\w+)', self.show('status'))
        return None if m is None else m.group(1).lower() == 'enabled'

    def set_ap(self, ssid: str, password: str) -> None:
        self.cmd(f'set_ap {ssid} {password}')

    def set_ap_channel(self, channel: int) -> None:
        """0 - авто, 1..13 - фиксированный. Только сборка с аплинком Ethernet."""
        self.cmd(f'set_ap_channel {channel}')

    def set_tx_power(self, dbm: int) -> None:
        self.cmd(f'set_tx_power {dbm}')

    def dhcp_reserve(self, mac: str, ip: str) -> None:
        """
        Закрепить адрес за устройством. Без этого правила фильтра пришлось бы
        переписывать после каждой выдачи адреса.

        Имя устройства прошивка не принимает: форма `-- <имя>` из вики проекта
        отвергается разбором аргументов (`excess option`).
        """
        self.cmd_checked(f'dhcp_reserve add {mac} {ip}',
                         lambda: self.dhcp_reservations().get(mac.lower()) == ip,
                         'резервирование адреса')

    def dhcp_reservations(self) -> dict[str, str]:
        """MAC -> адрес, как их печатает `show mappings`."""
        out = self.show('mappings')
        return dict(
            (m.group(1).lower(), m.group(2))
            for m in re.finditer(r'([0-9a-fA-F:]{17})\s*->\s*(\d+\.\d+\.\d+\.\d+)', out))

    def client_stats(self, enabled: bool = True) -> None:
        self.cmd(f'client_stats {"enable" if enabled else "disable"}')

    # --- фильтр ----------------------------------------------------------

    def acl_add(self, acl_list: str, rule: str) -> None:
        """
        Завести правило: `acl <список> <proto> <src> [<порт>] <dst> [<порт>]
        <действие>`. Слова `add` в синтаксисе нет - с ним прошивка молча
        ничего не делает.
        """
        before = len(self.acl_rules(acl_list))
        self.cmd_checked(f'acl {acl_list} {rule}',
                         lambda: len(self.acl_rules(acl_list)) > before,
                         'правило фильтра')

    def acl_clear(self) -> None:
        """Снять все правила во всех списках."""
        for acl_list in ACL_LISTS:
            if self.acl_rules(acl_list):
                self.cmd_checked(f'acl {acl_list} clear',
                                 lambda name=acl_list: not self.acl_rules(name),
                                 f'очистка списка {acl_list}')

    def acl_stats(self, acl_list: str) -> dict[str, int]:
        """
        Счётчики списка из `show acl`: allowed, denied, no_match.

        По ним видно, что фильтр действительно тронул трафик. Само по себе
        заведённое правило не значит ничего: с неверным списком оно так же
        читается и так же ничего не режет.
        """
        out: dict[str, int] = {}
        current = None
        for line in self.show('acl').splitlines():
            header = re.match(r'ACL:\s*(\w+)', line.strip())
            if header:
                current = header.group(1)
                continue
            if current != acl_list:
                continue
            m = re.search(r'allowed=(\d+), denied=(\d+), no_match=(\d+)', line)
            if m:
                out = {'allowed': int(m.group(1)), 'denied': int(m.group(2)),
                       'no_match': int(m.group(3))}
        return out

    def acl_rules(self, acl_list: str | None = None) -> list[str]:
        """
        Правила из `show acl`. Прошивка печатает их пронумерованными строками
        внутри блока `ACL: <список>`, а не в том виде, в каком они заводились.
        """
        rules: list[str] = []
        current = None
        for line in self.show('acl').splitlines():
            header = re.match(r'ACL:\s*(\w+)', line.strip())
            if header:
                current = header.group(1)
                continue
            if current is None or (acl_list and current != acl_list):
                continue
            body = line.strip()
            if re.match(r'\d+\s+(IP|TCP|UDP|ICMP)\b', body):
                rules.append(f'{current} {body}')
        return rules

    # --- сценарии --------------------------------------------------------

    # Списки названы от лица интерфейса, а не клиента, и это ровно наоборот
    # тому, что подсказывает имя: трафик, который клиент точки отправляет
    # наружу, приходит на интерфейс точки, а обработчик входа сверяется со
    # списком `to_ap` (netif_hooks.c: ap_netif_input_hook -> ACL_TO_AP;
    # ap_netif_linkoutput_hook -> ACL_FROM_AP). Правило в `from_ap` для адреса
    # устройства не совпадёт никогда - там источником стоит удалённый узел, а
    # не Ватериус, и фильтр молча ничего не режет.
    CLIENT_OUT = 'to_ap'

    def block_all(self, ip: str) -> None:
        """Интернета нет, Wi-Fi живёт: тесты G4, F4, D2b."""
        self.acl_add(self.CLIENT_OUT, f'IP {ip} any deny')

    def block_port(self, ip: str, port: int) -> None:
        """Один получатель недоступен, остальные живы: F2, F3, G5."""
        self.acl_add(self.CLIENT_OUT, f'TCP {ip} * any {port} deny')

    def block_udp_port(self, ip: str, port: int) -> None:
        """То же для UDP: нужен блоку N, там получатель - NTP."""
        self.acl_add(self.CLIENT_OUT, f'UDP {ip} * any {port} deny')

    @contextmanager
    def ap_off(self) -> Iterator[None]:
        """
        Погасить точку доступа и поднять обратно - с перезагрузкой платы.

        Перезагрузка здесь не перестраховка, а обход ошибки прошивки: в сборке
        с проводным аплинком NAT после `ap enable` не возвращается, и клиенты
        точки видят только её саму. Разбор и патч - 03_router-nat-bug.md.

        Стоит это около 12 секунд (замеряно: консоль отвечает через 5-6 с,
        клиент выходит наружу через 7-9 с). Без неё следующий тест упал бы,
        обвинив прошивку Ватериуса в том, что она не доставила посылку.
        """
        self.ap(False)
        try:
            yield
        finally:
            self.ap(True)
            self.restart()

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
        was = self.config().get('channel', '0')
        self.set_ap_channel(number)
        self.restart()
        try:
            yield
        finally:
            self.set_ap_channel(int(was))
            self.restart()

    @contextmanager
    def ssid(self, name: str, password: str) -> Iterator[None]:
        was = self.config().get('ssid', '')
        assert was, 'роутер не сообщил имя точки: восстанавливать нечем'
        restore_password = self._ap_password()
        self.set_ap(name, password)
        self.restart()
        try:
            yield
        finally:
            self.set_ap(was, restore_password)
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

        if state.ssid and now.get('ssid') != state.ssid:
            self.set_ap(state.ssid, self._ap_password())
            need_restart = True

        if state.channel and now.get('channel') != state.channel:
            self.set_ap_channel(int(state.channel))
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
            password: str = '', ap_password: str = '') -> NatRouter:
    """Собрать роутер поверх порта или поверх сетевой консоли."""
    if port:
        return NatRouter(SerialTransport(port), ap_password)
    if host:
        return NatRouter(TcpTransport(host, password), ap_password)
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
    p_nat = sub.add_parser('nat', help='NAT точки: без аргумента - состояние')
    p_nat.add_argument('state', nargs='?', choices=['on', 'off'])
    sub.add_parser('restart', help='перезагрузить плату')
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
        elif args.action == 'nat':
            if args.state is None:
                print('NAT:', router.nat_enabled())
            else:
                # Настройка ложится в NVS, а применяется при старте: без
                # перезагрузки клиенты по-прежнему не выйдут наружу.
                router.set_nat(args.state == 'on')
                router.restart()
        elif args.action == 'restart':
            router.restart()
        elif args.action == 'show':
            print(router.show(args.section))
        elif args.action == 'raw':
            print(router.cmd(args.line))
    finally:
        router.close()


if __name__ == '__main__':
    main()
