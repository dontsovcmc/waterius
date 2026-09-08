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
RE_ESP_VER = re.compile(r'Firmware ver: (\d+)\.(\d+)\.(\d+)')
RE_MAC = re.compile(r'MAC Address:\s*((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})')
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
# Значение, прошедшее валидацию и попавшее в настройки. Единственный
# способ проверить параметр, которого нет в посылке: адрес брокера, порт,
# включённость получателя.
RE_SAVED = re.compile(r'Saved: (\S+)=(\S*)')

# Настройки, напечатанные при загрузке (config.cpp: print_settings). Секции
# идут заголовками, а `state=` и `host=` внутри них называются одинаково -
# поэтому разбор идёт с оглядкой на текущую секцию, а не по одной строке.
RE_SECTION = re.compile(r'--- (\S+) ---')
SECTION_KEYS = {'Waterius.ru': 'waterius', 'HTTP': 'http', 'MQTT': 'mqtt'}
RE_STATE = re.compile(r'\bstate=(ON|OFF)\b')
RE_HOST = re.compile(r'\bhost=(\S*)')
# У брокера порт печатается той же строкой, что и адрес (config.cpp).
RE_PORT = re.compile(r'\bport=(\d+)')
RE_WIFI_SSID = re.compile(r'\bwifi_ssid=(\S*)')

SESSION_END = 'Going to sleep'

# Первая строка нового включения ЕСП. Нужна как вторая граница сеанса: сеанс
# режима настройки заканчивается перезапуском и `Going to sleep` не печатает,
# поэтому по одной строке засыпания два пробуждения склеивались в одно, а с
# фильтром по режиму нужный сеанс выбрасывался вместе с чужим.
RE_BOOT = re.compile(r'\bChipId: ')


@dataclass
class Session:
    """Один сеанс ЕСП: от включения питания attiny до `Going to sleep`."""

    lines: list[str] = field(default_factory=list)
    # Строки до `Startup mode:`: баннер загрузки и напечатанные настройки. Они
    # относятся к этому же пробуждению, но в сеанс не входят - утверждения
    # пишутся про сеанс, а не про то, что было до него.
    preamble: list[str] = field(default_factory=list)
    payload: dict[str, Any] | None = None          # посылка, пойманная приёмником
    payloads: list[dict[str, Any]] = field(default_factory=list)
    mqtt: list[tuple[str, str, bool]] = field(default_factory=list)

    # --- разобранные поля ---

    @property
    def text(self) -> str:
        return '\n'.join(self.lines)

    @property
    def full_text(self) -> str:
        """
        Всё пробуждение целиком, вместе с преамбулой.

        Часть фактов о себе прошивка печатает до `Startup mode:` - версии,
        MAC, настройки и итог загрузки конфига. Утверждения о ходе сеанса
        пишутся про `text`, а вот эти факты искать надо здесь.
        """
        return '\n'.join(self.preamble + self.lines)

    @property
    def mode(self) -> int | None:
        m = RE_MODE.search(self.text)
        return int(m.group(1)) if m else None

    @property
    def attiny_version(self) -> int | None:
        m = RE_ATTINY_VER.search(self.full_text)
        return int(m.group(1)) if m else None

    @property
    def esp_version(self) -> tuple[int, int, int] | None:
        """Версия прошивки ЕСП кортежем - чтобы сравнивать, а не сличать строки."""
        m = RE_ESP_VER.search(self.full_text)
        return (int(m.group(1)), int(m.group(2)), int(m.group(3))) if m else None

    @property
    def mac(self) -> str | None:
        """MAC устройства. Прошивка печатает его в каждом сеансе со связью."""
        m = RE_MAC.search(self.full_text)
        return m.group(1).lower() if m else None

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
    def config(self) -> dict[str, str]:
        """
        Настройки устройства, как оно само их напечатало при загрузке.

        Печатаются они до `Startup mode:`, то есть в преамбуле, а не в сеансе.

        Нужны, чтобы стенд мог заметить чужую сеть или чужой сервер до первого
        теста: спрашивать об этом человека - значит однажды прогнать весь набор
        против домашнего роутера и разбираться, почему приёмник пуст.
        """
        out: dict[str, str] = {}
        section = ''
        for line in self.preamble + self.lines:
            m = RE_SECTION.search(line)
            if m:
                section = SECTION_KEYS.get(m.group(1), '')
                continue
            ssid = RE_WIFI_SSID.search(line)
            if ssid:
                out['wifi_ssid'] = ssid.group(1)
                continue
            if not section:
                continue
            state = RE_STATE.search(line)
            if state:
                out[f'{section}_on'] = '1' if state.group(1) == 'ON' else '0'
                continue
            host = RE_HOST.search(line)
            if host:
                out[f'{section}_host'] = host.group(1)
                port = RE_PORT.search(line)
                if port:
                    out[f'{section}_port'] = port.group(1)
        return out

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
    def saved(self) -> dict[str, str]:
        """
        Настройки, которые прошивка приняла и записала.

        Отличается от `applied` тем, что там - полученное, а здесь - выжившее
        после валидации: отвергнутый параметр печатается ошибкой, и строки
        `Saved:` для него не будет.
        """
        return dict(RE_SAVED.findall(self.text))

    @property
    def complete(self) -> bool:
        return SESSION_END in self.text

    @property
    def blink_cause(self) -> str:
        """
        Причина вспышек светодиода. Сам код в лог не печатается, поэтому
        восстанавливаем его по входным условиям blink_code (core/blink.cpp).
        Единственное, что так не увидеть, - одна вспышка про просевшее питание.

        Причины `cloud`, `cloud_answer` и `mqtt` читаются из строки
        `Alarm confirm`, а её печатают с 2.0.47. На младших прошивках нет ни
        строки, ни самой модели причин, и результат вырождается в `ok` -
        поэтому тесты, утверждающие эти причины, помечены версией.
        """
        # Итог загрузки конфига и связь с attiny печатаются до `Startup mode:`,
        # то есть в преамбуле: искать их в тексте сеанса - значит объявить
        # неисправным конфиг в каждом сеансе подряд.
        if ('Config succesfully loaded' not in self.full_text
                or 'Attiny not found.' in self.full_text):
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

    Берём сырой текст (`serial_read`) и склеиваем сами: кольцо METF режет
    длинные строки на куски.

    Полноту лога сверяем со счётчиком потерь платы (`serial_stat`, METF 5 и
    клиент 0.4). Вытеснение молчаливое: лог приходит короче, а не с ошибкой,
    поэтому любое утверждение о его содержимом после потери ничего не стоит -
    отсюда падение теста, а не предупреждение в отчёт.
    """

    def __init__(self, api: Any) -> None:
        self.api = api
        self.lines: list[str] = []
        self._tail = ''
        self._can_stat: bool | None = None      # None - ещё не спрашивали

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

    # --- потери лога ---

    def _dropped(self) -> int | None:
        """
        Счётчик вытесненных строк с платы; None - счётчика нет.

        METF считает потери с последнего `flush()`, то есть значение
        накопительное, и смысл имеет только его прирост за окно наблюдения.
        Клиент 0.3 метода не знает, прошивка младше 5 не отвечает JSON - в обоих
        случаях проверка выключается один раз, с объяснением в логе.
        """
        if self._can_stat is False:
            return None
        try:
            dropped = int(self.api.serial_stat()['dropped'])
        except AttributeError:
            logger.warning('metf_python_client 0.3: потери лога не проверяются, '
                           'нужен 0.4 с serial_stat()')
            self._can_stat = False
            return None
        except Exception as err:                     # прошивка младше 5 или плата молчит
            logger.warning(f'METF не отдал /read/stat, потери лога не проверяются: {err}')
            self._can_stat = False
            return None
        self._can_stat = True
        return dropped

    def loss_mark(self) -> int | None:
        """Снимок счётчика перед ожиданием - опора для `assert_no_loss`."""
        return self._dropped()

    def assert_no_loss(self, mark: int | None, context: str) -> None:
        """
        Убедиться, что за окно наблюдения кольцо METF ничего не выбросило.

        Это причина, по которой тест падает не там, где ломается: из кольца
        уезжает `Startup mode:`, сеанс не собирается, и виноватым выглядит
        устройство. Поэтому проверка стоит и на удачном исходе, и на таймауте.
        """
        if mark is None:
            return
        now = self._dropped()
        if now is None:
            return
        lost = now - mark
        assert lost <= 0, (
            f'METF потерял {lost} строк лога за {context}: кольцо переполнилось, '
            f'и лог неполон - утверждать по нему нечего. Читайте чаще или '
            f'соберите прошивку платы с большим ASB_BUFFER_BYTES')

    def wait_session(self, timeout: float, mode: int | None = None,
                     poll_interval: float = 0.1) -> Session | None:
        """
        Дождаться завершённого сеанса. Началом считаем `Startup mode:`, концом -
        `Going to sleep`: только так видно, что ЕСП дошла до конца, а не была
        обесточена посреди отправки по таймауту attiny.

        Опрашиваем непрерывно: сеанс с автодискавери печатает сотни строк, а
        кольцо METF хоть и вмещает их (511 строк по 128 символов, полный сеанс -
        193), но за секунду молчания успевает набрать лишнего. Один опрос стоит
        15 мс, так что десять раз в секунду - это не нагрузка.

        По краям окна снимаем счётчик потерь: если кольцо всё-таки переполнилось,
        и «сеанс пришёл», и «сеанса не было» - утверждения ни о чём.
        """
        mark = self.loss_mark()
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.poll()
            session = self._take_session(mode)
            if session:
                self.assert_no_loss(mark, 'ожидание сеанса')
                return session
            time.sleep(poll_interval)
        self.assert_no_loss(mark, f'ожидание сеанса {timeout:.0f} с')
        return None

    def expect_no_session(self, timeout: float, mode: int | None = None,
                          poll_interval: float = 0.5) -> bool:
        """
        Убедиться, что сеанса не было. С mode - что не было сеанса именно этого
        вида: в тестах квитанции важно, что устройство не будит себя по тревоге,
        а плановые пробуждения при этом идут своим чередом.

        Тишину подтверждаем только по полному логу: потерянные строки - ровно то
        место, где мог быть пропущенный сеанс.
        """
        mark = self.loss_mark()
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.poll()
            for line in self.lines:
                m = RE_MODE.search(line)
                if m and (mode is None or int(m.group(1)) == mode):
                    return False
            time.sleep(poll_interval)
        self.assert_no_loss(mark, f'ожидание тишины {timeout:.0f} с')
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

        session = Session(lines=self.lines[start:end + 1],
                          preamble=self.lines[:start])
        del self.lines[:end + 1]
        return session

    def _find_end(self, start: int) -> int | None:
        """
        Конец сеанса: строка засыпания либо начало следующего включения.

        Второй случай - не редкость: из режима настройки прошивка уходит
        перезапуском (`ESP.restart()` в main.cpp), и строки про сон там не
        будет никогда.
        """
        for i in range(start, len(self.lines)):
            if SESSION_END in self.lines[i]:
                return i
            if i > start and (RE_BOOT.search(self.lines[i])
                              or RE_MODE.search(self.lines[i])):
                return i - 1        # дальше уже следующее пробуждение
        return None
