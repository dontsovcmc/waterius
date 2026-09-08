"""
Разбор лога проверяется без железа.

Смысл этих тестов - не «регулярное выражение написано», а «строка из прошивки
разбирается в том виде, в каком она приезжает со стенда». А приезжает она
кусками: плата METF режет строки по 60 символов, из которых 27 занимает
префикс лога Ватериуса. Поэтому образцы здесь режутся ровно так же.
"""

from __future__ import annotations

from .logwatch import ALARM_MODE, LogWatcher, TRANSMIT_MODE

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
    fw('Alarm config: interval0=0 leak0=0 interval1=40 leak1=120 vacation=0'),
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
    assert session.alarm_config == {'interval0': 0, 'leak0': 0, 'interval1': 40,
                                    'leak1': 120, 'vacation': 0}
    assert session.confirm == {'mask': 4, 'waterius': 1, 'http': 0, 'mqtt': 3,
                               'any': 1, 'confirmed': 0}
    assert session.http_codes == [200]
    assert session.period_attiny == 4
    assert session.wifi_connected


def test_причина_вспышек_восстанавливается() -> None:
    """Код в лог не печатается - причина считается по тем же входным условиям."""
    watcher = LogWatcher(FakeApi(through_ring(SESSION_ALARM)))
    watcher.poll()
    session = watcher._take_session(None)
    assert session is not None
    # mqtt=3 (нет связи с брокером) при живом облаке - четыре вспышки
    assert session.blink_cause == 'mqtt'

    no_wifi = LogWatcher(FakeApi(through_ring([
        fw('Startup mode: 3'),
        fw('Config succesfully loaded'),
        fw('WIFI: Connection failed.', level='ERROR'),
        fw('WIFI: Connection failed.20123 ms', level='ERROR'),
        fw('Going to sleep'),
    ])))
    no_wifi.poll()
    session = no_wifi._take_session(None)
    assert session is not None
    assert session.blink_cause == 'router'


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
