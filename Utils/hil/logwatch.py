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

import requests
from loguru import logger

# Осечка связи, а не отказ платы. Свой список, а не импорт из metf.py: разбор
# лога проверяется без железа, и тянуть сюда клиент METF незачем
NETWORK_ERRORS = (requests.RequestException, OSError)

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

# Коды ошибок = число вспышек красного светодиода (core/blink.h, ErrorBlynks).
# Стенд их не считает, а только читает выбранный прошивкой код из лога.
BLYNK_LOW_VOLTAGE = 1
BLYNK_ROUTER = 2
BLYNK_CLOUD = 3          # и облако, и свой сервер: у них общий код
BLYNK_MQTT = 4
BLYNK_CONFIG = 5
BLYNK_CLOUD_ANSWER = 6

BLYNK_NAMES = {
    BLYNK_LOW_VOLTAGE: 'низкое питание',
    BLYNK_ROUTER: 'нет роутера',
    BLYNK_CLOUD: 'облако или свой сервер',
    BLYNK_MQTT: 'брокер',
    BLYNK_CONFIG: 'настройки',
    BLYNK_CLOUD_ANSWER: 'ответ облака',
}

# Коды про отправку. Отделены от остальных, потому что проверки отправки
# сверялись с «моргания нет вовсе», а низкое питание (код 1) - беда стенда, и
# от неё падали тесты с текстом про получателей, которые были ни при чём
SENDING_BLYNKS = (BLYNK_ROUTER, BLYNK_CLOUD, BLYNK_MQTT, BLYNK_CLOUD_ANSWER)


def blynk_name(code: int | None) -> str:
    """Код моргания словами: в тексте падения «3» ничего не объясняет."""
    if code is None:
        return 'молчит'
    return f'код {code} ({BLYNK_NAMES.get(code, "неизвестный")})'

RE_MODE = re.compile(r'Startup mode: (\d)')
RE_ATTINY_VER = re.compile(r'attiny firmware ver: (\d+)')
RE_ESP_VER = re.compile(r'Firmware ver: (\d+)\.(\d+)\.(\d+)')
RE_MAC = re.compile(r'MAC Address:\s*((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})')
RE_IMP0 = re.compile(r'\bimp0:(\d+)')
RE_IMP1 = re.compile(r'\bimp1:(\d+)')
RE_ALARM_CONFIG = re.compile(
    r'Alarm config: quantum0=(\d+) quanta0=(\d+) vol0=(\d+)'
    r' quantum1=(\d+) quanta1=(\d+) vol1=(\d+)'
    r' vacation=([01]) reset=(\d+)')
RE_ALARM_CONFIRM = re.compile(
    r'Alarm confirm: mask=(\d+) waterius=(\d) http=(\d) mqtt=(\d) any=([01]) -> ([01])')
RE_IDLE_MIN = re.compile(r'Idle min: (\d+)/(\d+), stop: ([01])/([01])')
RE_IDLE_SEND = re.compile(r'Idle: consumed=([01]), silence_min=(\d+), transmit=([01])')
RE_HTTP_CODE = re.compile(r'HTTP: Response code: (-?\d+)')

# Публикация в MQTT глазами самой прошивки: по строке на топик и итог. Нужны,
# чтобы отличить «устройство не опубликовало» от «брокер не получил»: раньше
# отказ говорил только про брокер, и виноватым выглядело устройство.
RE_MQTT_PUB = re.compile(r'MQTT: pub (\S+) size=(\d+) retain=([01])')
RE_MQTT_DONE = re.compile(r'MQTT: Publish data finished: (\d+) topics, (\d+) ms')
RE_MQTT_FAIL = re.compile(r'MQTT: Publish failed: (\S+) \(([^)]*)\)')
# Код ошибки, который прошивка собралась моргать (wleds.cpp, blynk_error).
# Успех не моргается вовсе, поэтому строки в удачном сеансе нет.
RE_BLYNK = re.compile(r'Blynk: code=(\d+)')
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
SECTION_KEYS = {'Waterius.ru': 'waterius', 'HTTP': 'http', 'MQTT': 'mqtt',
                'Network': 'network'}
# Остальные поля секций - именами параметров прошивки: их нет в посылке, и
# без них стенд не знал бы, выполнены ли требования теста
SECTION_FIELDS = {
    'waterius': (('waterius_email', re.compile(r'\bemail=(\S*)')),),
    'mqtt': (('mqtt_auto_discovery', re.compile(r'\bauto discovery=(\d)')),
             ('mqtt_retain', re.compile(r'\bretain=(\d)'))),
    'network': (('ntp_server', re.compile(r'\bntp_server=(\S*)')),),
}
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

# Стартовый лог ЕСП (main.cpp: setup, master_i2c.cpp) печатается до разговора с
# attiny: по нему видно, что ЕСП жива и чем прошита, даже когда сеанса не будет.
RE_ESP_BOOT_VER = re.compile(r'ESP firmware ver: (\S+)')
RE_BUILD = re.compile(r'Build: (.+?)\s*$')
RE_ERROR = re.compile(r'\bERROR\s*:\s*(.+?)\s*$')
ATTINY_LOST = 'Attiny not found.'

# Чем кончилось пробуждение по кнопке (LogWatcher.wait_wake)
WAKE_SESSION = 'session'        # attiny ответила, сеанс начался
WAKE_NO_ATTINY = 'no_attiny'    # ЕСП жива, attiny молчит на i2c
WAKE_SILENT = 'silent'          # на UART ни строки
WAKE_STUCK = 'stuck'            # ЕСП печатает, но ни сеанса, ни ошибки attiny


@dataclass
class Wake:
    """Стартовый лог одного пробуждения: жива ли ЕСП и ответила ли ей attiny."""

    state: str
    lines: list[str]
    waited: float
    raw: str = ''       # всё, что отдала METF, включая недописанную строку

    def dump(self) -> str:
        """Всё прочитанное платой после нажатия, как есть."""
        if not self.raw:
            return 'METF не отдала ни байта'
        if not self.lines:
            return f'METF отдала {len(self.raw)} байт, ни одной полной строки: {self.raw!r}'
        return f'METF отдала {len(self.raw)} байт:\n{self.raw.rstrip()}'

    def _first(self, pattern: re.Pattern[str]) -> str | None:
        for line in self.lines:
            m = pattern.search(line)
            if m:
                return m.group(1)
        return None

    @property
    def esp_version(self) -> str | None:
        return self._first(RE_ESP_BOOT_VER)

    @property
    def build(self) -> str | None:
        return self._first(RE_BUILD)

    @property
    def errors(self) -> list[str]:
        return [m.group(1) for m in map(RE_ERROR.search, self.lines) if m]

    @property
    def blynk(self) -> int | None:
        code = self._first(RE_BLYNK)
        return int(code) if code is not None else None

    def describe(self, button: str) -> str:
        """Что сказать человеку, когда сеанс не начался. button - чем нажимали."""
        if self.state == WAKE_NO_ATTINY:
            blynk = f', Blynk: code={self.blynk}' if self.blynk is not None else ''
            return (f'ЕСП жива и прошита (ЕСП {self.esp_version or "?"}, сборка '
                    f'{self.build or "?"}), но attiny не отвечает по i2c: '
                    f'{"; ".join(self.errors)}{blynk}. Проверьте прошивку и фьюзы '
                    f'attiny - docs/flashing.md\n{self.dump()}')
        if self.state == WAKE_SILENT:
            return (f'Ватериус не проснулся: за {self.waited:.1f} с после нажатия '
                    f'{button} на UART ни одной строки. Кнопка не доходит до '
                    f'attiny, нет питания, или к UART ЕСП подключён программатор - '
                    f'он держит её в загрузчике\n{self.dump()}')
        if self.state == WAKE_STUCK:
            errors = f', ошибки: {"; ".join(self.errors)}' if self.errors else ''
            return (f'ЕСП печатает, но сеанс не начался: за {self.waited:.1f} с '
                    f'ни `Startup mode:`, ни `{ATTINY_LOST}`{errors}\n{self.dump()}')
        return 'сеанс начался'


# Как часто опрашивается лог платы. С протокола 12 плата держит окно до
# подтверждения, поэтому потерянный ответ ничего не стоит - и частый опрос
# перестал быть риском. Осталась только его польза: чем чаще читаем, тем
# меньше лежит в кольце.
#
# А кольцо - узкое место: 64 КБ (`ASB_BUFFER_BYTES`), это ~511 строк при пике
# 222 строки/с (публикация в MQTT, 05_air-and-loss.md), то есть 2,3 с потока.
# Замер 25 сентября: одна заминка связи на 1,4 с стоила 217 строк, и кольцо
# переполнялось. Пятая доля секунды держит в кольце ~44 строки, так что запас
# остаётся даже с непрочитанным окном и заминкой чтения.
#
# Один опрос стоит около 15 мс, пять раз в секунду - это не нагрузка.
POLL_INTERVAL = 0.2


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
    # Сколько тел посылок приехало оборванными. Проставляет `stand.py`,
    # wait_session: без этого числа тест, недосчитавшийся посылки, винит
    # прошивку в том, что сделал эфир.
    broken: int = 0

    # --- разобранные поля ---

    @property
    def air_note(self) -> str:
        """
        Приписка про эфир к отказу, где посылок пришло меньше ожидаемого.

        Обрыв тела рождается в эфире стенда, а не в устройстве: гибнет один
        сегмент, буфер отправки ЕСП не освобождается, и клиент сдаётся
        (05_air-and-loss.md). Приёмнику такая посылка не достаётся вовсе -
        отсюда недосчитанная посылка в тесте.
        """
        if not self.broken:
            return ''
        return (f'\nПри этом {self.broken} тел(а) посылок приехали оборванными: '
                f'эфир стенда теряет сегмент, и недостающая посылка - про него, '
                f'а не про прошивку (05_air-and-loss.md)')

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

        Печатается уже в единицах attiny: длина кванта тишины в тиках по 250мс,
        сколько таких квантов подряд считать протечкой и сколько импульсов за
        полчаса считать прорывом. Человеческие л/ч и часы сюда не доезжают -
        пересчёт остаётся в ЕСП, и стенд проверяет то, что реально уехало.
        """
        m = RE_ALARM_CONFIG.search(self.text)
        if not m:
            return None
        return {'quantum0': int(m.group(1)), 'quanta0': int(m.group(2)),
                'vol0': int(m.group(3)),
                'quantum1': int(m.group(4)), 'quanta1': int(m.group(5)),
                'vol1': int(m.group(6)),
                'vacation': int(m.group(7)), 'reset': int(m.group(8))}

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
            for name, regex in SECTION_FIELDS.get(section, ()):
                found = regex.search(line)
                if found:
                    out[name] = found.group(1)
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
    def mqtt_published(self) -> list[tuple[str, int, bool]]:
        """
        Что прошивка доложила об опубликованном: топик, размер, retain.

        Это её собственный взгляд, а не взгляд брокера, и в этом вся польза:
        при расхождении сразу видно, чья беда. Прошивка старше одной строки на
        публикацию ничего такого не печатает - тогда список пуст, и утверждать
        по нему нечего.
        """
        return [(topic, int(size), flag == '1')
                for topic, size, flag in RE_MQTT_PUB.findall(self.text)]

    @property
    def mqtt_failed(self) -> list[tuple[str, str]]:
        """Топики, о которых прошивка сказала, что публикация не удалась."""
        return RE_MQTT_FAIL.findall(self.text)

    @property
    def mqtt_done(self) -> tuple[int, int] | None:
        """Итог публикации показаний: сколько топиков и за сколько миллисекунд."""
        m = RE_MQTT_DONE.search(self.text)
        return (int(m.group(1)), int(m.group(2))) if m else None

    @property
    def mqtt_note(self) -> str:
        """
        Приписка к отказу про MQTT: что об этом говорит само устройство.

        Отказ «в брокере пусто» без неё обвиняет прошивку в том, что могло быть
        бедой стенда - потерей подписки, обрывом связи с брокером.
        """
        published = self.mqtt_published
        if not published and self.mqtt_done is None:
            return ('\nУстройство о публикации ничего не сказало: либо оно не '
                    'дошло до MQTT, либо на нём прошивка старше строки '
                    '«MQTT: pub»')
        parts = []
        if self.mqtt_done is not None:
            topics, ms = self.mqtt_done
            parts.append(f'устройство отчиталось о {topics} топиках за {ms} мс')
        if published:
            parts.append(f'в логе {len(published)} публикаций, последняя - '
                         f'{published[-1][0]}')
        if self.mqtt_failed:
            parts.append(f'неудачные: {self.mqtt_failed}')
        return '\nЧто говорит устройство: ' + '; '.join(parts)

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

    # --- утверждения на языке предметной области ---

    @property
    def blynk(self) -> int | None:
        """
        Код ошибки, который прошивка собралась моргать. None - не моргала.

        Это слово самого устройства, а не наша реконструкция по условиям:
        восстановленный стендом код согласился бы с ошибкой, если ошибка в
        самой модели. Число вспышек отсюда не следует - стенд их не видит.
        """
        m = RE_BLYNK.search(self.text)
        return int(m.group(1)) if m else None

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
        # Всё прочитанное с последнего clear() как есть: недописанная строка и
        # мусор в lines не попадают, а при отказе показывать надо и их
        self.raw = ''
        self._can_stat: bool | None = None      # None - ещё не спрашивали
        # Удачные и неудачные чтения лога. По ним видно, слеп ли стенд: пока
        # METF не отвечает, пустой лог не говорит об устройстве ничего
        self.reads_ok = 0
        self.reads_failed = 0
        self._mark_at: float | None = None      # начало окна наблюдения потерь

    def poll(self) -> None:
        """Забрать накопленное с платы и склеить разрезанные строки."""
        try:
            chunk = self.api.serial_read()
        except Exception as err:                     # плата могла не ответить
            self.reads_failed += 1
            logger.warning(f'METF не отдал лог: {err}')
            return
        self.reads_ok += 1
        if not chunk:
            return
        self.raw += chunk

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
        self.raw = ''

    # --- потери лога ---

    def _dropped(self) -> int | None:
        """
        Счётчик вытесненных строк с платы; None - счётчика нет.

        METF считает потери с последнего `flush()`, то есть значение
        накопительное, и смысл имеет только его прирост за окно наблюдения.
        Клиент 0.3 метода не знает, прошивка младше 5 не отвечает JSON - в обоих
        случаях проверка выключается один раз, с объяснением в логе. Обрыв связи
        к таким причинам не относится: он временный, и проверку не гасит.
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
            # Выключаем проверку только если причина постоянная. Обрыв связи
            # временный: METF ходит по радио и отваливается, а раньше одна
            # такая осечка молча гасила проверку потерь до конца прогона
            if not isinstance(err, NETWORK_ERRORS):
                logger.warning(
                    f'METF не отдал /read/stat, потери лога не проверяются: {err}')
                self._can_stat = False
            else:
                logger.warning(f'METF не отдал /read/stat в этот раз: {err}')
            return None
        self._can_stat = True
        return dropped

    def loss_mark(self) -> int | None:
        """Снимок счётчика перед ожиданием - опора для `assert_no_loss`."""
        self._mark_at = time.time()
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
            f'соберите прошивку платы с большим ASB_BUFFER_BYTES'
            f'{self._why_lost()}')

    def _why_lost(self) -> str:
        """
        Приписка о том, кто виноват в потере, если это известно.

        Кольцо METF держит 511 строк, а сеанс с включённым MQTT печатает около
        тысячи: прошивка кладёт каждое поле в свой топик и на каждую публикацию
        печатает пять строк (замер 24 сентября 2026 - 05_air-and-loss.md). Лог
        живёт только непрерывным чтением, а связь с платой идёт по радио и
        пропадает сама по себе. Без этой приписки отлучку ищут в прошивке
        устройства, хотя устройство тут ни при чём.
        """
        if self._mark_at is None:
            return ''

        beda = []
        reboot = getattr(self.api, 'reboot_since', None)
        when = reboot(self._mark_at) if reboot else None
        if when is not None:
            beda.append(f'METF перезагружалась в '
                        f'{time.strftime("%H:%M:%S", time.localtime(when))} - '
                        f'кольцо она при этом очищает')

        stall = getattr(self.api, 'stall_since', None)
        gap = stall(self._mark_at) if stall else None
        if gap is not None:
            since, seconds = gap
            beda.append(f'связь с METF пропадала на {seconds:.1f} с '
                        f'({time.strftime("%H:%M:%S", time.localtime(since))})')

        if not beda:
            return ''
        return (f'. В этом окне {"; ".join(beda)} - виноват стенд, а не прошивка')

    def wait_session(self, timeout: float, mode: int | None = None,
                     poll_interval: float = POLL_INTERVAL) -> Session | None:
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

    def wait_start(self, timeout: float) -> bool:
        """
        Дождаться первых строк на UART устройства.

        По кнопке устройство просыпается сразу, поэтому тишина здесь - отказ
        самого устройства, а не повод ждать дальше. Слепоту стенда с ней путать
        нельзя: пока METF не отдала ни одного чтения, о Ватериусе не известно
        ничего, и приговор выносить нечем.
        """
        reads, failed = self.reads_ok, self.reads_failed
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.poll()
            if self.lines:
                return True
            time.sleep(POLL_INTERVAL)
        assert self.reads_ok > reads, (
            f'стенд ослеп: за {timeout:.1f} с METF ни разу не отдала лог '
            f'({self.reads_failed - failed} неудачных чтений), поэтому о '
            f'пробуждении Ватериуса сказать нечего')
        return False

    def wait_line(self, text: str, timeout: float,
                  poll_interval: float = 0.5) -> float | None:
        """
        Дождаться строки в логе и вернуть, сколько секунд её ждали.

        Нужно там, где событие не сеанс: портал закрывается по сторожевому
        таймеру и печатает про это одну строку. Время возвращается потому, что
        в таких проверках важно не только «случилось», но и «не раньше».
        """
        mark = self.loss_mark()
        started = time.time()
        deadline = started + timeout
        while time.time() < deadline:
            self.poll()
            if any(text in line for line in self.lines):
                self.assert_no_loss(mark, f'ожидание строки {text!r}')
                return time.time() - started
            time.sleep(poll_interval)
        self.assert_no_loss(mark, f'ожидание строки {text!r} {timeout:.0f} с')
        return None

    def wait_wake(self, timeout: float, poll_interval: float = POLL_INTERVAL) -> Wake:
        """
        Разобрать стартовый лог после нажатия кнопки, не досиживая таймаут.

        `Startup mode:` ЕСП печатает только после ответа attiny (main.cpp:
        loop), поэтому одна эта строка не отличает живую ЕСП без attiny от
        отсутствующего устройства. Решает первая строка, которая определяет
        исход; таймаут - только для молчания.
        """
        mark = self.loss_mark()
        reads, reads_failed = self.reads_ok, self.reads_failed
        started = time.time()
        while True:
            self.poll()
            waited = time.time() - started
            state = self._wake_state()
            if state is None and waited >= timeout:
                # Молчание на UART - приговор устройству, и выносить его, не
                # прочитав ни разу, нельзя: пока METF не отвечала, о Ватериусе
                # не известно ничего. Прежде стенд в этом случае писал
                # «Ватериус не проснулся» и советовал искать кнопку, питание и
                # программатор - при исправном устройстве.
                assert self.reads_ok > reads, (
                    f'стенд ослеп: за {waited:.1f} с METF ни разу не отдала лог '
                    f'({self.reads_failed - reads_failed} неудачных чтений), '
                    f'поэтому о пробуждении Ватериуса сказать нечего')
                state = WAKE_STUCK if self.lines else WAKE_SILENT
            if state is not None:
                if state == WAKE_NO_ATTINY:
                    # `Blynk: code=` идёт следом через 4 мс - в этот опрос или в следующий
                    time.sleep(poll_interval)
                    self.poll()
                self.assert_no_loss(mark, 'стартовый лог')
                return Wake(state, list(self.lines), waited, self.raw)
            time.sleep(poll_interval)

    def _wake_state(self) -> str | None:
        for line in self.lines:
            if RE_MODE.search(line):
                return WAKE_SESSION
            if ATTINY_LOST in line:
                return WAKE_NO_ATTINY
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
                    # Не тот сеанс: выбрасываем его целиком, чтобы не мешал.
                    # В журнал он всё-таки попадает: тест, ждущий сеанса, что
                    # именно устройство делало вместо него, иначе не расскажет.
                    end = self._find_end(i)
                    if end is None:
                        return None
                    logger.info(f'пропускаем сеанс mode={m.group(1)}, '
                                f'ждём mode={mode}')
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
