"""
Лог Ватериуса как объект: строки одного сеанса и разобранные из них поля.

Почему не regex прямо в тестах. Блоки отправки печатаются дважды за сеанс, если
сервер прислал настройки (main.cpp: apply_settings, затем повторный send_data),
а `HTTP: Send OK` одинаков для waterius.ru и для своего сервера. Искать строку по
всему буферу - значит рано или поздно поймать не ту. Поэтому буфер режется на
сеансы (от `Startup mode:` до `Going to sleep`), и утверждения пишутся про сеанс.

Склейка разрезанных строк. Плата METF хранит лог кольцом строк по 60 символов, а
префикс лога Ватериуса (`01:23:456-025/03%  INFO  : `) занимает 27 - на текст
остаётся 32, и почти каждая строка приезжает кусками. Куски узнаются по началу:
настоящая строка начинается с временной метки, продолжение - нет. Клиентский
параметр prefix='00:' делает то же самое, но перестаёт работать через минуту
после включения ЕСП, когда минуты в метке становятся не нулевыми.
"""

from __future__ import annotations

import re
import time
from dataclasses import dataclass, field
from typing import Any

from loguru import logger

# Начало настоящей строки лога: MM:SS:mmm (Logging.h, LOG_FORMAT_TIME).
LINE_START = re.compile(r'^\d{2}:\d{2}:\d{3}')

# Режимы пробуждения, core/types.h
SETUP_MODE = 1
TRANSMIT_MODE = 2
MANUAL_TRANSMIT_MODE = 3
ALARM_MODE = 4

# SendStatus, core/blink.h
SEND_SKIPPED = 0
SEND_OK = 1
SEND_BAD_ANSWER = 2
SEND_NO_CONNECTION = 3

RE_MODE = re.compile(r'Startup mode: (\d)')
RE_ATTINY_VER = re.compile(r'attiny firmware ver: (\d+)')
RE_IMP0 = re.compile(r'\bimp0:(\d+)')
RE_IMP1 = re.compile(r'\bimp1:(\d+)')
RE_ALARM_CONFIG = re.compile(
    r'Alarm config: interval0=(\d+) leak0=(\d+) interval1=(\d+) leak1=(\d+) vacation=([01])')
RE_ALARM_CONFIRM = re.compile(
    r'Alarm confirm: mask=(\d+) waterius=(\d) http=(\d) mqtt=(\d) any=([01]) -> ([01])')
RE_IDLE_MIN = re.compile(r'Idle min: (\d+)/(\d+), stop: ([01])/([01])')
RE_IDLE_SEND = re.compile(r'Idle: consumed=([01]), silence_min=(\d+), transmit=([01])')
RE_HTTP_CODE = re.compile(r'HTTP: Response code: (-?\d+)')
RE_PERIOD_ATTINY = re.compile(r'Wakeup period, min \(attiny\):(\d+)')
RE_APPLY = re.compile(r'Apply setting: (\S+)=(\S*)')

SESSION_END = 'Going to sleep'


@dataclass
class Session:
    """Один сеанс ЕСП: от включения питания attiny до `Going to sleep`."""

    lines: list[str] = field(default_factory=list)
    payload: dict[str, Any] | None = None          # посылка, пойманная приёмником
    payloads: list[dict[str, Any]] = field(default_factory=list)
    mqtt: list[tuple[str, str, bool]] = field(default_factory=list)

    # --- разобранные поля ---

    @property
    def text(self) -> str:
        return '\n'.join(self.lines)

    @property
    def mode(self) -> int | None:
        m = RE_MODE.search(self.text)
        return int(m.group(1)) if m else None

    @property
    def attiny_version(self) -> int | None:
        m = RE_ATTINY_VER.search(self.text)
        return int(m.group(1)) if m else None

    @property
    def impulses(self) -> tuple[int | None, int | None]:
        m0 = RE_IMP0.search(self.text)
        m1 = RE_IMP1.search(self.text)
        return (int(m0.group(1)) if m0 else None,
                int(m1.group(1)) if m1 else None)

    @property
    def alarm_config(self) -> dict[str, int] | None:
        """
        Пороги, уехавшие в ОЗУ attiny. Отсутствие строки означает, что тревоги
        настроить не удалось (старая attiny) - без неё вся группа E бессмысленна.
        """
        m = RE_ALARM_CONFIG.search(self.text)
        if not m:
            return None
        return {'interval0': int(m.group(1)), 'leak0': int(m.group(2)),
                'interval1': int(m.group(3)), 'leak1': int(m.group(4)),
                'vacation': int(m.group(5))}

    @property
    def confirm(self) -> dict[str, int] | None:
        """
        Квитанция тревоги и заодно статусы всех трёх получателей числами.
        Единственная строка, по которой виден итог сеанса целиком; печатается
        только при удачном Wi-Fi.
        """
        m = RE_ALARM_CONFIRM.search(self.text)
        if not m:
            return None
        return {'mask': int(m.group(1)), 'waterius': int(m.group(2)),
                'http': int(m.group(3)), 'mqtt': int(m.group(4)),
                'any': int(m.group(5)), 'confirmed': int(m.group(6))}

    @property
    def idle(self) -> dict[str, int] | None:
        m = RE_IDLE_MIN.search(self.text)
        if not m:
            return None
        return {'min0': int(m.group(1)), 'min1': int(m.group(2)),
                'stop0': int(m.group(3)), 'stop1': int(m.group(4))}

    @property
    def idle_send(self) -> dict[str, int] | None:
        m = RE_IDLE_SEND.search(self.text)
        if not m:
            return None
        return {'consumed': int(m.group(1)), 'silence_min': int(m.group(2)),
                'transmit': int(m.group(3))}

    @property
    def wifi_connected(self) -> bool:
        return 'WIFI: Connected.' in self.text

    @property
    def http_codes(self) -> list[int]:
        return [int(x) for x in RE_HTTP_CODE.findall(self.text)]

    @property
    def period_attiny(self) -> int | None:
        m = RE_PERIOD_ATTINY.search(self.text)
        return int(m.group(1)) if m else None

    @property
    def applied(self) -> dict[str, str]:
        """Настройки, приехавшие в ответе сервера или по MQTT."""
        return dict(RE_APPLY.findall(self.text))

    @property
    def complete(self) -> bool:
        return SESSION_END in self.text

    @property
    def blink_cause(self) -> str:
        """
        Причина вспышек светодиода. Сам код в лог не печатается, поэтому
        восстанавливаем его по входным условиям blink_code (core/blink.cpp).
        Единственное, что так не увидеть, - одна вспышка про просевшее питание.
        """
        if 'Config succesfully loaded' not in self.text or 'Attiny not found.' in self.text:
            return 'config'          # 5 вспышек
        if not self.wifi_connected:
            return 'router'          # 2 вспышки
        c = self.confirm
        if c:
            cloud = max(c['waterius'], c['http'])
            if cloud == SEND_NO_CONNECTION:
                return 'cloud'       # 3 вспышки
            if cloud == SEND_BAD_ANSWER:
                return 'cloud_answer'  # 6 вспышек
            if c['mqtt'] in (SEND_BAD_ANSWER, SEND_NO_CONNECTION):
                return 'mqtt'        # 4 вспышки
        return 'ok'

    # --- утверждения на языке предметной области ---

    def assert_alarm(self, **expected: int) -> None:
        """assert_alarm(flow1=1, flow0=0) - по полям посылки."""
        assert self.payload is not None, 'посылки не было, проверять нечего'
        for name, want in expected.items():
            got = self.payload.get(f'alarm_{name}')
            assert got == want, f'alarm_{name}: ожидали {want}, получили {got}\n{self.text}'

    def assert_confirm(self, **expected: int) -> None:
        c = self.confirm
        assert c is not None, f'в сеансе нет строки Alarm confirm\n{self.text}'
        for name, want in expected.items():
            assert c[name] == want, f'{name}: ожидали {want}, получили {c[name]}\n{self.text}'

    def assert_cause(self, cause: str) -> None:
        got = self.blink_cause
        assert got == cause, f'причина сеанса: ожидали {cause}, получили {got}\n{self.text}'

    def assert_delta(self, channel: int, liters: int) -> None:
        assert self.payload is not None, 'посылки не было'
        got = self.payload.get(f'delta{channel}')
        assert got == liters, f'delta{channel}: ожидали {liters}, получили {got}'


class LogWatcher:
    """
    Читает UART Ватериуса через плату METF и нарезает поток на сеансы.

    Работает с клиентом 0.3: берём сырой текст (`serial_read`) и склеиваем
    сами. Как только в METF появится /read/stat, сюда добавится проверка на
    потерянные строки - молчаливая потеря делает тест ложно-зелёным.
    """

    def __init__(self, api: Any) -> None:
        self.api = api
        self.lines: list[str] = []
        self._tail = ''

    def poll(self) -> None:
        """Забрать накопленное с платы и склеить разрезанные строки."""
        try:
            chunk = self.api.serial_read()
        except Exception as err:                     # плата могла не ответить
            logger.warning(f'METF не отдал лог: {err}')
            return
        if not chunk:
            return

        raw = (self._tail + chunk).split('\n')
        # Последний кусок может быть незавершённым - придержим до следующего раза
        self._tail = raw.pop() if not chunk.endswith('\n') else ''

        for piece in raw:
            piece = piece.rstrip('\r')
            if not piece:
                continue
            if LINE_START.match(piece) or not self.lines:
                self.lines.append(piece)
            else:
                # Продолжение строки, разрезанной кольцом METF
                self.lines[-1] += piece

    def clear(self) -> None:
        self.poll()
        self.lines.clear()
        self._tail = ''

    def wait_session(self, timeout: float, mode: int | None = None,
                     poll_interval: float = 1.0) -> Session | None:
        """
        Дождаться завершённого сеанса. Началом считаем `Startup mode:`, концом -
        `Going to sleep`: только так видно, что ЕСП дошла до конца, а не была
        обесточена посреди отправки по таймауту attiny.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.poll()
            session = self._take_session(mode)
            if session:
                return session
            time.sleep(poll_interval)
        return None

    def expect_no_session(self, timeout: float, mode: int | None = None,
                          poll_interval: float = 2.0) -> bool:
        """
        Убедиться, что сеанса не было. С mode - что не было сеанса именно этого
        вида: в тестах квитанции важно, что устройство не будит себя по тревоге,
        а плановые пробуждения при этом идут своим чередом.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.poll()
            for line in self.lines:
                m = RE_MODE.search(line)
                if m and (mode is None or int(m.group(1)) == mode):
                    return False
            time.sleep(poll_interval)
        return True

    def _take_session(self, mode: int | None) -> Session | None:
        start = None
        for i, line in enumerate(self.lines):
            m = RE_MODE.search(line)
            if m:
                if mode is not None and int(m.group(1)) != mode:
                    # Не тот сеанс: выбрасываем его целиком, чтобы не мешал
                    end = self._find_end(i)
                    if end is None:
                        return None
                    del self.lines[:end + 1]
                    return self._take_session(mode)
                start = i
                break
        if start is None:
            return None

        end = self._find_end(start)
        if end is None:
            return None

        session = Session(lines=self.lines[start:end + 1])
        del self.lines[:end + 1]
        return session

    def _find_end(self, start: int) -> int | None:
        for i in range(start, len(self.lines)):
            if SESSION_END in self.lines[i]:
                return i
        return None
