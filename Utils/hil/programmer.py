"""
Программатор waterius_flasher на гребёнке Ватериуса: воздействия и лог ESP по USB.

Та же прищепка, которой плату прошивают. Команды — строки `#…\\n`, на каждую одна строка
ответа (протокол — README репозитория waterius_flasher, раздел «Стенд HIL», спека
`bench-control`). Линии называются ролями: `rst`, `ch0`, `ch1`, `button`; уровень `1` в
пачке — линия отпущена, а не выведена в 1.

Повторов нет: USB посылок не теряет, поэтому молчание или обрыв порта значат, что
программатор пропал или завис. Это `ProgrammerGone`, и прогон останавливается: без него
стенд не может ни нажать кнопку, ни прочитать лог.
"""

from __future__ import annotations

import codecs
import json
import re
import threading
import time
from collections.abc import Mapping, Sequence
from typing import Any, Protocol

import serial
from loguru import logger
from serial.tools import list_ports

LINES = ('rst', 'ch0', 'ch1', 'button')

# Espressif: USB Serial/JTAG ESP32-C6
USB_VID = 0x303A
BAUD = 115200
REPLY_S = 2.0
# Первая версия с `#wave`, `#hold`, `#release` (спека bench-control)
MIN_VERSION = (0, 7, 0)
# Не больше стольких байт лога в одном ответе `#log` (waterius_flasher, core/port.h: LOG_CHUNK)
LOG_CHUNK = 4096
LOG_BAUD = 115200

RE_INFO = re.compile(r'^# waterius_flasher (\d+)\.(\d+)\.(\d+)\S* uptime=(\d+)')
RE_LOG = re.compile(r'^# log: (\d+) ([01])$')
RE_WAVE = re.compile(r'^# wave: (\d+)$')


class ProgrammerGone(Exception):
    """Программатор не ответил или порт пропал: прогон продолжать бессмысленно."""


class ProgrammerTooOld(Exception):
    """Прошивка программатора старше той, на которую рассчитан стенд."""


class NoBoard(Exception):
    """Программатор на месте, а платы на прищепке нет."""


class WaveRefused(ValueError):
    """Программатор отказал в заказе: текст отказа — его."""


class LineBusy(WaveRefused):
    """На линии идёт пачка: удержание с неё не снять, пока пачка не кончится."""


class Link(Protocol):
    """Сколько от `serial.Serial` нужно клиенту: в самотестах её подменяют."""

    dtr: bool

    def write(self, data: bytes) -> int | None: ...

    def readline(self) -> bytes: ...

    def read(self, size: int) -> bytes: ...

    def reset_input_buffer(self) -> None: ...

    def close(self) -> None: ...


def open_link(port: str) -> serial.Serial:
    """
    Открыть порт так, как его держит страница производства.

    DTR=0 при RTS=1 перезагружает ESP32-C6 (CLAUDE.md waterius_flasher), а `open()` pyserial
    снимает DTR раньше RTS. `dsrdtr=True` убирает DTR из `open()`, и DTR снимается после RTS
    (замер 28.11: иначе каждое открытие порта перезагружало программатор).
    """
    link = serial.Serial()
    link.port = port
    link.baudrate = BAUD
    link.timeout = REPLY_S
    link.rts = False
    link.dsrdtr = True
    link.open()
    link.dtr = False
    time.sleep(0.3)
    link.reset_input_buffer()
    return link


class Programmer:
    """Команды программатора. Один замок на порт: команда и её ответ не перемешиваются."""

    def __init__(self, link: Link, port: str = '') -> None:
        self._link = link
        self.port = port
        self.version: tuple[int, int, int] | None = None
        self._lock = threading.Lock()
        self._decoder = codecs.getincrementaldecoder('utf-8')(errors='replace')

    @classmethod
    def open(cls, port: str) -> Programmer:
        try:
            return cls(open_link(port), port)
        except (serial.SerialException, OSError) as err:
            raise ProgrammerGone(f'программатор: порт {port} не открылся: {err}') from err

    def close(self) -> None:
        try:
            self._link.close()
        except (serial.SerialException, OSError):
            pass

    # --- основа ---

    def _gone(self, what: str, err: BaseException | None = None) -> ProgrammerGone:
        tail = f': {err}' if err else ''
        return ProgrammerGone(
            f'стенд: программатор {self.port or "?"} {what}{tail}. Без него стенд не может '
            f'ни нажать кнопку, ни прочитать лог: проверьте кабель USB и `#info` в порт')

    def _line(self, sent: str) -> str:
        try:
            raw = self._link.readline()
        except (serial.SerialException, OSError) as err:
            raise self._gone(f'пропал на «{sent}»', err) from err
        if not raw.endswith(b'\n'):
            raise self._gone(f'не ответил на «{sent}» за {REPLY_S:.0f} с')
        return raw.decode(errors='replace').rstrip('\r\n')

    def _send(self, line: str) -> None:
        try:
            self._link.write(line.encode() + b'\n')
        except (serial.SerialException, OSError) as err:
            raise self._gone(f'пропал на «{line}»', err) from err

    def cmd(self, line: str) -> str:
        """Команда и строка ответа без перевода строки."""
        with self._lock:
            self._send(line)
            return self._line(line)

    # --- состояние ---

    def info(self) -> tuple[tuple[int, int, int], int]:
        """Версия прошивки и время с пуска, с."""
        answer = self.cmd('#info')
        m = RE_INFO.match(answer)
        if m is None:
            raise self._gone(f'ответил на «#info» не по-своему: {answer!r}')
        self.version = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
        return self.version, int(m.group(4))

    def board(self) -> bool:
        """Плата на прищепке: по уровню отпущенного RST."""
        answer = self.cmd('#board')
        if answer not in ('# board: есть', '# board: нет'):
            raise self._gone(f'ответил на «#board» не по-своему: {answer!r}')
        return answer == '# board: есть'

    def wake(self) -> None:
        """
        Достучаться до программатора, оставшегося в мосте к ESP после прошивки.

        В мосте команды не разбираются, выход — смена DTR (design `esp8266-bridge` §3).
        RTS при этом 0, иначе смена DTR перезагрузит программатор.
        """
        try:
            self.info()
            return
        except ProgrammerGone:
            pass
        logger.warning(f'программатор {self.port} молчит на «#info»: выхожу из моста сменой DTR')
        try:
            self._link.dtr = True
            time.sleep(0.1)
            self._link.dtr = False
            time.sleep(0.5)
            self._link.reset_input_buffer()
        except (serial.SerialException, OSError) as err:
            raise self._gone('пропал при выходе из моста', err) from err
        self.info()

    # --- линии ---

    def lines_off(self) -> None:
        """Все линии отпущены, приём лога закрыт."""
        self._expect('#lines off', '# lines: отпущены')

    def wave(self, lines: Sequence[Mapping[str, Any]]) -> int:
        """
        Пачка фронтов одной командой; ответ — сразу, до конца пачки.

        @param lines строки `{'line': 'ch1', 'value': 0, 'at_ms': 0, 'edges': [500, 1500, …]}`
        @return длина пачки, мс
        """
        parts = ['#wave']
        for line in lines:
            parts += [str(line['line']), str(int(line.get('value', 0))),
                      str(int(line.get('at_ms', 0))),
                      ','.join(str(int(edge)) for edge in line['edges'])]
        command = ' '.join(parts)
        answer = self.cmd(command)
        m = RE_WAVE.match(answer)
        logger.info(f'пачка «{command[6:]}»: {answer[2:]}')
        if m is None:
            raise WaveRefused(f'программатор отказал в пачке «{command}»: {answer}')
        return int(m.group(1))

    def wave_stat(self) -> dict[str, Any]:
        """Расписка последней начатой пачки: моменты фронтов по часам программатора."""
        answer = self.cmd('#wave stat')
        if not answer.startswith('# wave: {'):
            raise self._gone(f'ответил на «#wave stat» не JSON: {answer!r}')
        return dict(json.loads(answer[len('# wave: '):]))

    def hold(self, line: str) -> None:
        """Линия в 0 до `release`."""
        self._line_cmd('hold', line)

    def release(self, line: str) -> None:
        """Отпустить удержанную линию; свободную — ничего. На линии с пачкой — `LineBusy`."""
        self._line_cmd('release', line)

    def _line_cmd(self, cmd: str, line: str) -> None:
        answer = self.cmd(f'#{cmd} {line}')
        if answer == f'# {cmd}: {line}':
            return
        if answer == f'# {cmd}: линия занята: {line}':
            raise LineBusy(f'программатор: {cmd} {line} — на линии идёт пачка')
        raise WaveRefused(f'программатор отказал в «#{cmd} {line}»: {answer}')

    def _expect(self, command: str, want: str) -> None:
        answer = self.cmd(command)
        if answer != want:
            raise self._gone(f'ответил на «{command}» {answer!r}, а не {want!r}')

    # --- лог ESP ---

    def log_begin(self) -> None:
        """
        Открыть приём лога ESP с чистого кольца.

        Сперва `#lines off`: программатор мог остаться в сеансе ISP или в мосте после
        прошивки, а `#esp log` линий не трогает.
        """
        self.lines_off()
        self._expect(f'#esp log {LOG_BAUD}', f'# esp: лог {LOG_BAUD}')
        self._decoder.reset()

    def log_read(self) -> tuple[str, bool]:
        """
        Всё, что лежит в кольце, и были ли потери с прошлого чтения.

        Ответ `#log` — не больше `LOG_CHUNK` байт, поэтому читаем, пока ответ полный: за один
        вызов кольцо выбирается до дна.
        """
        chunks: list[bytes] = []
        lost = False
        with self._lock:
            while True:
                self._send('#log')
                head = self._line('#log')
                m = RE_LOG.match(head)
                if m is None:
                    raise self._gone(f'ответил на «#log» {head!r}')
                n = int(m.group(1))
                lost |= m.group(2) == '1'
                try:
                    data = self._link.read(n) if n else b''
                except (serial.SerialException, OSError) as err:
                    raise self._gone('пропал посреди лога', err) from err
                if len(data) != n:
                    raise self._gone(f'отдал {len(data)} байт лога из обещанных {n}')
                chunks.append(data)
                if n < LOG_CHUNK:
                    break
        return self._decoder.decode(b''.join(chunks)), lost


def find_port(written: str = '') -> Programmer:
    """
    Программатор: записанный порт, иначе — тот порт Espressif, что отвечает как программатор.

    Найденный порт в stand.ini не пишется: имя `/dev/cu.usbmodem…` зависит от гнезда.
    """
    tried: list[str] = []
    candidates = [written] if written else []
    candidates += [p.device for p in list_ports.comports()
                   if p.vid == USB_VID and p.device != written]
    for port in candidates:
        tried.append(port)
        try:
            prog = Programmer.open(port)
            prog.wake()
        except ProgrammerGone as err:
            logger.warning(f'программатор: {port} — не он ({err})')
            continue
        if port != written:
            logger.info(f'программатор нашёлся на {port}'
                        + (f' ({written} не отвечает)' if written else ''))
        return prog
    raise ProgrammerGone(
        'стенд: программатора нет — ни один порт USB Espressif (VID 303a) не ответил на '
        f'«#info» (пробовал: {", ".join(tried) or "портов нет"}). Подключите программатор '
        'или задайте [programmer] port в stand.ini')


def check(prog: Programmer) -> str:
    """Досмотр перед прогоном: версия и плата на прищепке. Строка — для лога прогона."""
    version, uptime = prog.info()
    shown = '.'.join(map(str, version))
    if version < MIN_VERSION:
        raise ProgrammerTooOld(
            f'стенд: программатор {prog.port} — прошивка {shown}, а нужна '
            f'{".".join(map(str, MIN_VERSION))} и выше: до неё нет `#wave`, `#hold` и '
            f'`#release`. Прошейте программатор (README waterius_flasher)')
    if not prog.board():
        raise NoBoard(
            f'стенд: программатор {prog.port} на месте, а платы на прищепке нет '
            f'(`#board`). Проверьте прищепку на гребёнке Ватериуса')
    summary = f'{prog.port}, прошивка {shown}, с пуска {uptime} с, плата на прищепке'
    logger.info(f'стенд: программатор {summary}')
    return summary
