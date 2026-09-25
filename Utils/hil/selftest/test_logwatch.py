"""
Разбор лога проверяется без железа.

Смысл этих тестов - не «регулярное выражение написано», а «строка из прошивки
разбирается в том виде, в каком она приезжает со стенда». А приезжает она
кусками: плата METF режет строки по 60 символов, из которых 27 занимает
префикс лога Ватериуса. Поэтому образцы здесь режутся ровно так же.
"""

from __future__ import annotations

import time

import pytest

from ..logwatch import (
    ALARM_MODE,
    TRANSMIT_MODE,
    WAKE_NO_ATTINY,
    WAKE_SESSION,
    WAKE_SILENT,
    WAKE_STUCK,
    LogWatcher,
)

# Префикс из Logging.h при включённом LOG_FREE_HEAP: MM:SS:mmm-KKK/FF%  INFO  :
PREFIX = '01:23:456-025/03%  INFO  : '
RING_LINE_LEN = 60


def fw(text: str, level: str = 'INFO ') -> str:
    """Строка так, как её печатает прошивка."""
    return f'01:23:456-025/03%  {level} : {text}'


def through_ring(lines: list[str]) -> str:
    """
    Пропустить строки через кольцо METF: всё длиннее 59 символов разрезается,
    продолжение начинается с новой строки без префикса.
    """
    chunks: list[str] = []
    for line in lines:
        for i in range(0, len(line), RING_LINE_LEN - 1):
            chunks.append(line[i:i + RING_LINE_LEN - 1])
    return '\n'.join(chunks) + '\n'


class FakeApi:
    """Плата METF, отдающая заранее подготовленный лог."""

    def __init__(self, text: str) -> None:
        self._text = text

    def serial_read(self) -> str:
        text, self._text = self._text, ''
        return text


SESSION_ALARM = [
    fw('Startup mode: 4'),
    fw('attiny firmware ver: 41'),
    fw(' ctype1:0 imp1:1234 adc1:0'),
    fw('Config succesfully loaded'),
    fw('WIFI: Connected.'),
    fw('HTTP: Response code: 200'),
    fw('Alarm config: quantum0=0 quanta0=0 vol0=0'
       ' quantum1=3600 quanta1=4 vol1=5 vacation=0 reset=8'),
    fw('Alarm confirm: mask=4 waterius=1 http=0 mqtt=3 any=1 -> 0'),
    fw('Idle min: 0/0, stop: 0/0'),
    fw('Wakeup period, min (attiny):4'),
    fw('Going to sleep'),
]

SESSION_PLAN = [
    fw('Startup mode: 2'),
    fw('Config succesfully loaded'),
    fw('Idle: consumed=0, silence_min=15, transmit=0'),
    fw('Idle: no consumption, WiFi stays off'),
    fw('Going to sleep'),
]


def test_разрезанные_строки_склеиваются() -> None:
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM)))
    watcher.poll()

    assert len(watcher.lines) == len(SESSION_ALARM), (
        'строки должны склеиться обратно, а не остаться кусками')
    assert watcher.lines[-1].endswith('Going to sleep')


def test_сеанс_нарезается_целиком() -> None:
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM + SESSION_PLAN)))
    watcher.poll()

    first = watcher._take_session(None)
    assert first is not None and first.mode == ALARM_MODE
    assert first.complete

    second = watcher._take_session(None)
    assert second is not None and second.mode == TRANSMIT_MODE


def test_сеанс_выбирается_по_режиму() -> None:
    watcher = LogWatcher(FakeApi(through_ring(SESSION_PLAN + SESSION_ALARM)))
    watcher.poll()

    session = watcher._take_session(ALARM_MODE)
    assert session is not None and session.mode == ALARM_MODE


def test_поля_тревог_разбираются() -> None:
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM)))
    watcher.poll()
    session = watcher._take_session(None)
    assert session is not None

    assert session.attiny_version == 41
    assert session.impulses[1] == 1234
    assert session.alarm_config == {'quantum0': 0, 'quanta0': 0, 'vol0': 0,
                                    'quantum1': 3600, 'quanta1': 4, 'vol1': 5,
                                    'vacation': 0, 'reset': 8}
    assert session.confirm == {'mask': 4, 'waterius': 1, 'http': 0, 'mqtt': 3,
                               'any': 1, 'confirmed': 0}
    assert session.http_codes == [200]
    assert session.period_attiny == 4
    assert session.wifi_connected


def test_настройки_из_ответа_сервера_видны() -> None:
    watcher = LogWatcher(FakeApi(through_ring([
        fw('Startup mode: 3'),
        fw('Apply setting: vac=1'),
        fw('Apply setting: af1=1440'),
        fw('Going to sleep'),
    ])))
    watcher.poll()
    session = watcher._take_session(None)
    assert session is not None
    assert session.applied == {'vac': '1', 'af1': '1440'}


def test_незавершённый_сеанс_не_отдаётся() -> None:
    """
    Пока нет `Going to sleep`, сеанс не закончен: ЕСП могла быть обесточена
    посреди отправки по таймауту attiny, и судить по такому логу нельзя.
    """
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM[:-1])))
    watcher.poll()
    assert watcher._take_session(None) is None


SESSION_SETUP = [
    fw('Startup mode: 1'),
    fw('Entering in setup mode...'),
    fw('AP started on channel=6 , ssid=waterius-6827706-2.0.44'),
    fw('Shutdown HTTP and DNS servers'),
    fw('Restart ESP'),
]

BOOT = [
    fw('Waterius========'),
    fw('ChipId: 682eba'),
    fw('ESP firmware ver: 2.0.44'),
    fw('Config succesfully loaded'),
]


def test_сеанс_настройки_не_склеивается_со_следующим() -> None:
    """
    Из режима настройки прошивка уходит перезапуском и `Going to sleep` не
    печатает. Пока концом сеанса была только строка засыпания, два
    пробуждения слипались в одно, и утверждения читали факты не из того.
    """
    watcher = LogWatcher(FakeApi(through_ring(SESSION_SETUP + BOOT + SESSION_PLAN)))
    watcher.poll()

    setup = watcher._take_session(None)
    assert setup is not None and setup.mode == 1
    assert 'Restart ESP' in setup.text
    assert 'Startup mode: 2' not in setup.text

    following = watcher._take_session(None)
    assert following is not None and following.mode == TRANSMIT_MODE
    assert following.complete
    assert 'ChipId' in following.full_text, 'преамбула включения - у своего сеанса'


def test_сеанс_по_режиму_не_теряется_из_за_настройки() -> None:
    """
    С фильтром по режиму несовпавший сеанс выбрасывается целиком. Пока концом
    считалась только строка засыпания, вместе с сеансом настройки выбрасывался
    и следующий - тест ждал свой сеанс до таймаута и падал на живом стенде,
    обвиняя устройство.
    """
    watcher = LogWatcher(FakeApi(through_ring(SESSION_SETUP + BOOT + SESSION_PLAN)))
    watcher.poll()

    session = watcher._take_session(TRANSMIT_MODE)
    assert session is not None
    assert session.mode == TRANSMIT_MODE


# Настройки печатаются до `Startup mode:`, секциями, и `state=`/`host=` в них
# называются одинаково. Адрес брокера идёт одной строкой с портом.
SETTINGS_PRINT = [
    fw('wifi_ssid=waterius_stand'),
    fw('--- Waterius.ru ---- '),
    fw('state=OFF'),
    fw('host=cloud.waterius.ru key=xxx'),
    fw('--- HTTP ---- '),
    fw('state=ON'),
    fw('host=http://192.168.50.252:8000/data'),
    fw('--- MQTT ---- '),
    fw('state=ON'),
    fw('host=192.168.50.252 port=1883'),
    fw('login= pass='),
    fw('auto discovery=0'),
    fw('retain=1'),
    fw('discovery topic=homeassistant'),
    fw('--- Network ---- '),
    fw('DHCP is on'),
    fw('ntp_server=192.168.51.14'),
    fw('--- WIFI ---- '),
]


def test_настройки_разбираются_по_секциям() -> None:
    """
    Адрес брокера и его порт печатаются одной строкой, а `state=` и `host=`
    внутри секций называются одинаково: без оглядки на секцию сервер и брокер
    перепутались бы, и стенд настраивал бы устройство впустую.
    """
    watcher = LogWatcher(FakeApi(through_ring(SETTINGS_PRINT + SESSION_PLAN)))
    watcher.poll()
    session = watcher._take_session(None)
    assert session is not None
    config = session.config

    assert config['wifi_ssid'] == 'waterius_stand'
    assert config['waterius_on'] == '0'
    assert config['http_on'] == '1'
    assert config['http_host'] == 'http://192.168.50.252:8000/data'
    assert config['mqtt_on'] == '1'
    assert config['mqtt_host'] == '192.168.50.252'
    assert config['mqtt_port'] == '1883'
    # Этих полей нет в посылке, а требования тестов на них опираются
    assert config['mqtt_auto_discovery'] == '0'
    assert config['mqtt_retain'] == '1'
    assert config['ntp_server'] == '192.168.51.14'
    assert 'waterius_email' not in config, 'почты в образце нет'


def test_принятая_настройка_не_значит_сохранённая() -> None:
    """
    `Apply setting:` печатается до валидации, `Saved:` - после. Для параметров,
    которых нет в посылке (адрес брокера, порт, включённость получателя), это
    единственный способ отличить принятое от применённого.
    """
    lines = [
        fw('Startup mode: 2'),
        fw('Apply setting: mqtt_on=1'),
        fw('Saved: mqtt_on=1'),
        fw('Apply setting: mqtt_host=192.168.50.252'),
        fw('Saved: mqtt_host=192.168.50.252'),
        fw('Apply setting: mqtt_port=99999'),
        fw('Error: mqtt_port out of range', level='ERROR'),
        fw('Going to sleep'),
    ]
    watcher = LogWatcher(FakeApi(through_ring(lines)))
    watcher.poll()
    session = watcher._take_session(None)
    assert session is not None

    assert session.applied['mqtt_port'] == '99999', 'принято прошивкой'
    assert 'mqtt_port' not in session.saved, 'но не сохранено: значение отвергнуто'
    assert session.saved['mqtt_host'] == '192.168.50.252'


class BlindApi(FakeApi):
    """METF, которая не отвечает вовсе: стенд не видит ни строки."""

    def serial_read(self) -> str:
        raise ConnectionError('host is down')


def test_слепой_стенд_не_судит_об_устройстве() -> None:
    """
    METF молчала всё окно - о пробуждении Ватериуса сказать нечего.

    Прежде стенд писал «Ватериус не проснулся» и отправлял искать кнопку,
    питание и программатор, хотя устройство было исправно, а слеп был он сам.
    """
    watcher = LogWatcher(BlindApi(''))
    with pytest.raises(AssertionError, match='стенд ослеп'):
        watcher.wait_wake(timeout=0.3)


def test_молчание_при_живой_связи_остаётся_приговором() -> None:
    """METF отвечает, а на UART пусто - это уже про устройство."""
    watcher = LogWatcher(FakeApi(''))
    wake = watcher.wait_wake(timeout=0.3)
    assert wake.state == WAKE_SILENT
    assert 'не проснулся' in wake.describe('кнопки')


class CountingApi(FakeApi):
    """METF со счётчиком потерь: `dropped` растёт по заданному списку."""

    def __init__(self, text: str, dropped: list[int]) -> None:
        super().__init__(text)
        self._dropped = list(dropped)

    def serial_stat(self) -> dict:
        value = self._dropped.pop(0) if len(self._dropped) > 1 else self._dropped[0]
        return {'lines': 0, 'dropped': value, 'baud': 115200,
                'capacity': 511, 'line_len': 128, 'bytes': 65536}


def test_потеря_строк_валит_тест_а_не_прячется() -> None:
    """
    Кольцо переполнилось - сеанс собрался, но лог неполон.

    Это тот случай, ради которого счётчик и заводился: без него утверждения
    делаются по обрезанному логу, и тест зеленеет на дыре в нём.
    """
    watcher = LogWatcher(CountingApi(through_ring(SESSION_ALARM), [0, 17]))

    with pytest.raises(AssertionError, match='17'):
        watcher.wait_session(timeout=1.0)


class StallingApi(CountingApi):
    """METF, отлучившаяся посреди окна наблюдения: потеря объясняется отлучкой."""

    def __init__(self, text: str, dropped: list[int], seconds: float) -> None:
        super().__init__(text, dropped)
        self._seconds = seconds
        self._stall: tuple[float, float] | None = None

    def serial_read(self) -> str:
        # Отлучка случается внутри окна - иначе она к этой потере не относится
        self._stall = (time.time(), self._seconds)
        return super().serial_read()

    def stall_since(self, mark: float) -> tuple[float, float] | None:
        return self._stall if self._stall and self._stall[0] >= mark else None


def test_потеря_после_отлучки_платы_названа_отлучкой() -> None:
    """
    Кольцо METF держит треть секунды потока, а связь с платой идёт по радио:
    односекундная отлучка съедает лог целиком. Молчать об этом - значит послать
    человека искать поломку в прошивке устройства, которое всё сделало верно.
    """
    watcher = LogWatcher(StallingApi(through_ring(SESSION_ALARM), [0, 17], 1.1))
    with pytest.raises(AssertionError, match='1.1 с'):
        watcher.wait_session(timeout=1.0)


class RebootedApi(CountingApi):
    """METF, перезагрузившаяся посреди окна наблюдения."""

    def __init__(self, text: str, dropped: list[int]) -> None:
        super().__init__(text, dropped)
        self._reboot: float | None = None

    def serial_read(self) -> str:
        self._reboot = time.time()
        return super().serial_read()

    def reboot_since(self, mark: float) -> float | None:
        return self._reboot if self._reboot and self._reboot >= mark else None


def test_потеря_после_перезагрузки_платы_названа_перезагрузкой() -> None:
    """
    Перезагрузка METF объясняет пустое кольцо сама по себе: плата очищает его
    при старте. Без этой приписки потерю ищут в прошивке устройства.
    """
    watcher = LogWatcher(RebootedApi(through_ring(SESSION_ALARM), [0, 17]))
    with pytest.raises(AssertionError, match='METF перезагружалась'):
        watcher.wait_session(timeout=1.0)


def test_потеря_без_отлучки_ни_на_кого_не_кивает() -> None:
    """Связь не пропадала - значит причина другая, и выдумывать её нельзя."""
    watcher = LogWatcher(CountingApi(through_ring(SESSION_ALARM), [0, 17]))
    with pytest.raises(AssertionError) as err:
        watcher.wait_session(timeout=1.0)
    assert 'связь с METF пропадала' not in str(err.value)


def test_целый_лог_проверку_проходит() -> None:
    """Счётчик не вырос - сеанс отдаётся как обычно."""
    watcher = LogWatcher(CountingApi(through_ring(SESSION_ALARM), [4]))
    session = watcher.wait_session(timeout=1.0, mode=ALARM_MODE)
    assert session is not None
    assert session.alarm_config is not None


def test_старый_клиент_не_ломает_ожидание() -> None:
    """
    Клиент 0.3 не знает `serial_stat` - проверка выключается, а не падает.

    Иначе обновление прошивки платы стало бы обязательным для любого прогона.
    """
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM)))
    session = watcher.wait_session(timeout=1.0, mode=ALARM_MODE)
    assert session is not None


# Нажатие кнопки на стенде 13.09.2026: ЕСП прошита и жива, attiny на i2c не
# отвечает. Первые строки - обрывки загрузки, как их принял METF.
WAKE_NO_ATTINY_LOG = [
    'n',
    '========',
    '00:00:067-041/00%  INFO  : Build: Sep 13 2026 13:44:07',
    '00:00:068-041/00%  INFO  : IRAM free: 42376 bytes',
    '00:00:069-041/00%  INFO  : DRAM free: 42376 bytes',
    '00:00:074-041/00%  INFO  : ChipId: 682eba',
    '00:00:078-041/00%  INFO  : FlashChipId: 164020',
    '00:00:082-041/00%  INFO  : ESP firmware ver: 2.0.47',
    '00:00:087-041/00%  ERROR : end error:2',
    '00:00:090-041/00%  ERROR : Attiny not found.',
    '00:00:094-041/00%  INFO  : Blynk: code=5',
]


def test_живая_есп_без_attiny_называется_сразу() -> None:
    """
    `Startup mode:` печатается только после ответа attiny. Пока стенд ждал
    одну эту строку, живая ЕСП без attiny выглядела отсутствием устройства,
    и выяснялось это по таймауту.
    """
    watcher = LogWatcher(FakeApi(through_ring(WAKE_NO_ATTINY_LOG)))
    started = time.time()
    wake = watcher.wait_wake(timeout=2.0)

    assert time.time() - started < 1.0, 'решение - по строке, а не по таймауту'
    assert wake.state == WAKE_NO_ATTINY
    assert wake.esp_version == '2.0.47'
    assert wake.build == 'Sep 13 2026 13:44:07'
    assert wake.errors == ['end error:2', 'Attiny not found.']
    assert wake.blynk == 5
    message = wake.describe('кнопки')
    assert 'ЕСП жива' in message and 'attiny не отвечает' in message, message
    assert 'end error:2' in message and '2.0.47' in message, message


def test_молчание_отличается_от_живой_есп() -> None:
    wake = LogWatcher(FakeApi('')).wait_wake(timeout=0.3)
    assert wake.state == WAKE_SILENT
    message = wake.describe('кнопки')
    assert 'ни одной строки' in message and 'ни байта' in message, message


def test_начало_сеанса_принимается_сразу() -> None:
    watcher = LogWatcher(FakeApi(through_ring(BOOT + SESSION_PLAN)))
    started = time.time()
    wake = watcher.wait_wake(timeout=2.0)
    assert wake.state == WAKE_SESSION
    assert time.time() - started < 1.0
    assert watcher._take_session(None) is not None, 'стартовый лог не съеден'


def test_лог_без_начала_сеанса_не_молчание() -> None:
    """Строки идут, но ни сеанса, ни ошибки attiny: показываем, что пришло."""
    wake = LogWatcher(FakeApi(through_ring(BOOT))).wait_wake(timeout=0.3)
    assert wake.state == WAKE_STUCK
    message = wake.describe('кнопки')
    assert 'Startup mode' in message and 'ESP firmware ver: 2.0.44' in message, message


def test_отказ_показывает_всё_прочитанное() -> None:
    """
    Вердикт без лога заставляет гадать. Недописанная строка в список строк не
    попадает, поэтому показывается сырьё целиком - кусок без перевода строки
    тоже.
    """
    wake = LogWatcher(FakeApi('n')).wait_wake(timeout=0.3)
    assert wake.state == WAKE_SILENT
    message = wake.describe('кнопки')
    assert "1 байт" in message and "'n'" in message, message

    wake = LogWatcher(FakeApi(through_ring(WAKE_NO_ATTINY_LOG))).wait_wake(timeout=2.0)
    message = wake.describe('кнопки')
    for line in WAKE_NO_ATTINY_LOG:
        assert line in message, f'нет строки {line!r}:\n{message}'


def test_оборванные_тела_приписываются_к_отказу() -> None:
    """Недосчитанная посылка обязана называть обрыв: это эфир, а не прошивка."""
    from ..logwatch import Session

    assert Session().air_note == ''
    note = Session(broken=3).air_note
    assert '3' in note and 'оборванными' in note and '05_air-and-loss.md' in note


def сеанс_с_публикацией() -> object:
    """Лог сеанса так, как его печатает прошивка: строка на топик и итог."""
    from ..logwatch import Session

    return Session(lines=[
        fw('MQTT: Connected.'),
        fw('MQTT: pub waterius/ch0 size=5 retain=1 heap=34560'),
        fw('MQTT: pub waterius/ch1 size=6 retain=1 heap=34544'),
        fw('MQTT: Publish failed: waterius/voltage (socket)', level='ERROR'),
        fw('MQTT: Publish data finished: 83 topics, 2406 ms'),
    ])


def test_публикации_разбираются_по_логу() -> None:
    """
    Взгляд самой прошивки на то, что она опубликовала: по нему отличают
    «устройство не опубликовало» от «брокер не получил».
    """
    session = сеанс_с_публикацией()

    assert session.mqtt_published == [
        ('waterius/ch0', 5, True),
        ('waterius/ch1', 6, True),
    ]
    assert session.mqtt_failed == [('waterius/voltage', 'socket')]
    assert session.mqtt_done == (83, 2406)


def test_приписка_называет_взгляд_устройства() -> None:
    note = сеанс_с_публикацией().mqtt_note

    assert '83 топиках' in note and '2406 мс' in note
    assert 'waterius/voltage' in note, note


def test_молчащий_лог_не_обвиняет_прошивку() -> None:
    """
    Прошивка старше одной строки на публикацию ничего такого не печатает -
    и утверждать по пустому списку нечего.
    """
    from ..logwatch import Session

    note = Session(lines=[fw('MQTT: Connected.')]).mqtt_note
    assert 'ничего не сказало' in note and 'старше' in note


class DeafApi(FakeApi):
    """METF, которая не отвечает вовсе: стенд слеп, а не устройство молчит."""

    def serial_read(self) -> str:
        raise TimeoutError('нет ответа')


def test_молчание_после_нажатия_видно_сразу() -> None:
    """
    По кнопке устройство просыпается сразу, поэтому пустой UART - готовый
    ответ, а не повод ждать дальше.
    """
    watcher = LogWatcher(FakeApi(''))
    assert watcher.wait_start(0.3) is False


def test_первые_строки_прекращают_ожидание() -> None:
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM)))
    assert watcher.wait_start(5.0) is True


def test_слепой_стенд_не_судит_о_пробуждении() -> None:
    """
    Пока METF не отдала ни одного чтения, о Ватериусе не известно ничего:
    «не проснулся» здесь было бы приговором наугад.
    """
    watcher = LogWatcher(DeafApi(''))
    with pytest.raises(AssertionError, match='ослеп'):
        watcher.wait_start(0.3)


# --- сеанс, у которого не доехало начало ---

# То же пробуждение, но без метки `Startup mode:` - ровно то, что видел стенд
# 25 сентября 2026: устройство отработало и ушло спать, а строка начала
# потерялась в приёмном буфере UART платы, до кольца
БЕЗ_НАЧАЛА = SESSION_ALARM[1:]


class OverrunApi(FakeApi):
    """METF, у которой растёт счётчик переполнений буфера UART."""

    def __init__(self, text: str, overruns: list[int]) -> None:
        super().__init__(text)
        self._overruns = list(overruns)

    def serial_stat(self) -> dict:
        value = (self._overruns.pop(0) if len(self._overruns) > 1
                 else self._overruns[0])
        return {'lines': 0, 'dropped': 0, 'overruns': value, 'baud': 115200,
                'capacity': 511, 'line_len': 128, 'bytes': 65536}


def test_сеанс_без_начала_всё_равно_сеанс() -> None:
    """
    Раньше стенд ждал метку начала до потолка и винил устройство, хотя в буфере
    лежал целый сеанс со строкой ухода в сон.
    """
    session = LogWatcher(FakeApi(through_ring(БЕЗ_НАЧАЛА))).wait_session(timeout=2.0)

    assert session is not None, 'сеанс без начала снова не собрался'
    assert session.headless
    assert session.mode is None, 'режим взять неоткуда - угадывать его нечем'
    assert 'Going to sleep' in session.text


def test_сеанс_без_начала_не_закрывает_ожидание_режима() -> None:
    """
    Опознать его нечем, поэтому выдать за тревожный нельзя: тест про тревогу
    позеленел бы на плановом сеансе.
    """
    watcher = LogWatcher(FakeApi(through_ring(БЕЗ_НАЧАЛА)))
    assert watcher.wait_session(timeout=1.0, mode=ALARM_MODE) is None
    assert watcher.headless_pending, 'улика для отказа потерялась'


def test_сеанс_без_начала_ломает_утверждение_о_тишине() -> None:
    """Не заметить его здесь значит позеленеть на тишине, которой не было."""
    watcher = LogWatcher(FakeApi(through_ring(БЕЗ_НАЧАЛА)))
    assert watcher.expect_no_session(timeout=1.0) is False


def test_переполнение_буфера_платы_валит_тест() -> None:
    """
    Дыра, которой не видит счётчик кольца: байты не дошли даже до него.
    Молчать о ней нельзя - именно она съедает начало сеанса.
    """
    watcher = LogWatcher(OverrunApi(through_ring(SESSION_ALARM), [0, 3]))

    with pytest.raises(AssertionError, match='приёмный буфер UART'):
        watcher.wait_session(timeout=1.0)


class StumblingApi(FakeApi):
    """METF, которая спотыкается на первых чтениях, а потом отвечает."""

    def __init__(self, text: str, промахов: int) -> None:
        super().__init__(text)
        self._промахов = промахов

    def serial_read(self) -> str:
        if self._промахов > 0:
            self._промахов -= 1
            time.sleep(0.4)          # чтение ждёт ответа и не дожидается
            raise TimeoutError('нет ответа')
        return super().serial_read()


def test_осечка_чтения_не_отнимает_время_у_устройства() -> None:
    """
    Окно приговора - две секунды, одно чтение ждёт секунду и повторяется:
    одна заминка связи съедала окно целиком, и стенд объявлял себя слепым на
    исправном устройстве. Теперь время неудачных чтений ему возвращается.
    """
    watcher = LogWatcher(StumblingApi(through_ring(SESSION_ALARM), промахов=2))
    assert watcher.wait_start(1.0) is True


def test_оборванный_лог_называет_последнюю_строку() -> None:
    """
    Прошивка внутри сетевого вызова молчит, и оборванный лог со стороны не
    отличить от потерянного. Отличает последняя строка и время с неё.
    """
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM[:5])))
    watcher.poll()
    watcher._line_at = time.time() - 130      # молчит дольше, чем живёт питание

    note = watcher.stuck_note()
    assert 'Устройство молчит 130 с' in note, note
    assert 'attiny держит питание' in note, note
    assert 'обесточена' in note, note


def test_свежий_лог_приписки_не_рождает() -> None:
    """Пока строки идут, объяснять нечего - приписка только мешала бы."""
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM[:5])))
    watcher.poll()
    assert watcher.stuck_note() == ''
