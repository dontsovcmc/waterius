"""
Клиент METF с повторами. Железо не нужно: плату заменяет заглушка.

Проверяется ровно то, ради чего обёртка написана: короткая осечка связи - повод
повторить, долгое молчание - повод остановить прогон, а ошибка не про связь
должна лететь как есть, без повторов и без задержек.
"""

from __future__ import annotations

from typing import Any

import pytest
import requests

from .. import metf


class Clock:
    """Часы под управлением теста: `sleep` двигает время, а не ждёт."""

    def __init__(self) -> None:
        self.now = 1000.0

    def time(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeAnswer:
    """Ответ платы: стенду нужны `raise_for_status`, код и заголовки."""

    def __init__(self, status_code: int = 202, uptime: int | None = None,
                 text: str = '0') -> None:
        self.status_code = status_code
        self.headers = {} if uptime is None else {'X-Uptime-Ms': str(uptime)}
        # Длина пачки в теле: по ней стенд знает, сколько ждать
        self.text = text

    def raise_for_status(self) -> None:
        return None


class FakeSession:
    """`requests.Session` на минималках: помнит запросы и падает по заказу."""

    def __init__(self, failures: int = 0, error: Exception | None = None,
                 status_code: int = 202, text: str = '0') -> None:
        self.failures = failures
        self.error = error or requests.ConnectionError('host is down')
        self.status_code = status_code
        self.text = text
        self.posts: list[tuple[str, dict, float]] = []
        # Крючки ответа - как у настоящей сессии: через них клиент ловит аптайм
        self.hooks: dict[str, list] = {'response': []}

    def post(self, url: str, timeout: float, data: dict | None = None,
             json: dict | None = None) -> FakeAnswer:
        # Пачка фронтов уходит телом JSON, одиночный импульс - формой
        self.posts.append((url, data if data is not None else json or {}, timeout))
        if len(self.posts) <= self.failures:
            raise self.error
        return self.hooked(FakeAnswer(self.status_code, text=self.text))

    def hooked(self, answer: Any) -> Any:
        """Ответ через крючки сессии - так же, как это делает requests."""
        for hook in self.hooks['response']:
            hook(answer)
        return answer


class FakeClient:
    """Плата, падающая заданное число раз подряд, а затем отвечающая."""

    def __init__(self, failures: int, error: Exception | None = None,
                 session: FakeSession | None = None) -> None:
        self.failures = failures
        self.error = error or requests.ConnectionError('host is down')
        self.calls = 0
        self._root = 'http://192.0.2.1'
        self._sess = session or FakeSession()

    def ping(self) -> str:
        self.calls += 1
        if self.calls <= self.failures:
            raise self.error
        return 'pong'


@pytest.fixture
def clock(monkeypatch: pytest.MonkeyPatch) -> Clock:
    fake = Clock()
    monkeypatch.setattr(metf, 'time', fake)
    return fake


def board(monkeypatch: pytest.MonkeyPatch, client: FakeClient, **kw: object) -> metf.Metf:
    monkeypatch.setattr(metf, 'METFClient', lambda host: client)
    return metf.Metf('192.0.2.1', **kw)      # type: ignore[arg-type]


def test_короткая_осечка_переживается_повтором(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Плата отвалилась на секунду - тест не должен из-за этого падать."""
    client = FakeClient(failures=1)
    api = board(monkeypatch, client)
    assert api.ping() == 'pong'
    assert client.calls == 2, 'повтора не было'


def test_молчание_дольше_срока_останавливает_прогон(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата не отвечает дольше отведённого - дальше идти незачем: без METF стенд
    не может ничего, и остальные тесты дадут тот же traceback.
    """
    client = FakeClient(failures=99)
    api = board(monkeypatch, client, attempts=5, pause=1.0, dead_after=2.0)
    with pytest.raises(metf.MetfGone) as err:
        api.ping()
    assert '192.0.2.1' in str(err.value)


def test_ошибка_не_про_связь_не_повторяется(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Повторять осмысленно обрыв связи, а не ошибку вызова."""
    client = FakeClient(failures=1, error=ValueError('плохой аргумент'))
    api = board(monkeypatch, client)
    with pytest.raises(ValueError):
        api.ping()
    assert client.calls == 1, 'ошибку вызова повторять нельзя'


def test_импульс_повторяется_если_соединение_не_установилось(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Плата не приняла соединение - запроса она не видела, можно повторить."""
    session = FakeSession(failures=1)
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    api.pulse(pin=1, value=0, duration_ms=500)
    assert len(session.posts) == 2, 'повтора не было'
    assert session.posts[0][0] == 'http://192.0.2.1/pulse'


def test_импульс_не_повторяется_после_read_timeout(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Запрос ушёл, ответа нет: импульс мог быть выдан. Повтор нажал бы кнопку
    второй раз - за длинным нажатием это режим настройки вместо передачи.
    """
    session = FakeSession(failures=99, error=requests.ReadTimeout('no answer'))
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    with pytest.raises(requests.ReadTimeout):
        api.pulse(pin=1, value=0, duration_ms=4000)
    assert len(session.posts) == 1, 'импульс повторять нельзя'


def test_нажатие_кнопки_идёт_через_обёртку(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Иначе самое частое действие стенда остаётся единственным без повторов."""
    from ..dut import Dut

    session = FakeSession(failures=1)
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    dut = Dut(api, button_pin=1, ch0_pin=3, ch1_pin=2, reset_pin=0)
    dut._led_ok = False           # индикатора у заглушки нет, он тут не проверяется
    dut.press_button()
    assert len(session.posts) == 2, 'нажатие прошло мимо повторов'


def test_выдержку_импульса_ждёт_стенд(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата отвечает сразу, и конец импульса - забота клиента. Не подождать
    значит тронуть ещё занятую линию: следующий импульс сольётся с этим.
    """
    session = FakeSession()
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    started = clock.now
    api.pulse(pin=1, value=0, duration_ms=2000)
    assert clock.now - started == pytest.approx(2.0), (
        'стенд не выдержал паузу до конца импульса')


def test_старая_прошивка_платы_видна_сразу(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    До протокола 8 плата отвечала концом импульса, и ответ опаздывал на четверть
    секунды. Промолчать об этом - значит отдать прогон с поехавшими паузами и
    падениями там, где прошивка Ватериуса ни при чём.
    """
    session = FakeSession(status_code=200)
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    with pytest.raises(metf.MetfTooOld) as err:
        api.pulse(pin=1, value=0, duration_ms=500)
    assert 'протокол' in str(err.value)


def test_пачка_уходит_одним_запросом_и_стенд_ждёт_её_длину(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Форму сигнала отмеряет плата - потому что интервалы между фронтами стенд
    может отмерить только через радио. Один запрос на всю пачку, и ждём ровно
    столько, сколько плата назвала в расписке.
    """
    session = FakeSession(text='1300')
    api = board(monkeypatch, FakeClient(failures=0, session=session))
    started = clock.now

    total = api.wave([{'pin': 2, 'value': 0, 'at_ms': 0, 'edges': [500, 300, 500]}])

    assert len(session.posts) == 1, f'запросов {len(session.posts)}, а нужен один'
    url, body, _ = session.posts[0]
    assert url == 'http://192.0.2.1/pulse'
    assert body['lines'][0]['edges'] == [500, 300, 500]
    assert total == pytest.approx(1.3)
    assert clock.now - started == pytest.approx(1.3), 'стенд не выждал пачку'


def test_негодную_пачку_плата_называет_словами(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Отказ 400 - это ошибка стенда, а не устройства, и она обязана дойти до
    человека текстом платы, а не общим «запрос не удался».
    """
    session = FakeSession(status_code=400, text='edge shorter than 1 ms')
    api = board(monkeypatch, FakeClient(failures=0, session=session))

    with pytest.raises(ValueError, match='edge shorter than 1 ms'):
        api.wave([{'pin': 2, 'value': 0, 'edges': [500, 0, 500]}])


def test_плата_без_формы_сигнала_видна_сразу(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Прошивка младше четырнадцатой на пачку ответит не распиской: продолжать с
    ней - значит подавать не то, что заказано, и падать не там, где сломалось.
    """
    session = FakeSession(status_code=200, text='0')
    api = board(monkeypatch, FakeClient(failures=0, session=session))

    with pytest.raises(metf.MetfTooOld, match='14'):
        api.wave([{'pin': 2, 'value': 0, 'edges': [500]}])


def test_пачка_повторяется_если_соединение_не_установилось(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """Плата не приняла соединение - запроса она не видела, можно повторить."""
    session = FakeSession(failures=1, text='500')
    api = board(monkeypatch, FakeClient(failures=0, session=session))

    api.wave([{'pin': 2, 'value': 0, 'edges': [500]}])

    assert len(session.posts) == 2, 'повтора не было'


class FakeBoard:
    """Плата, отвечающая заданным состоянием на `/version` и `/wifi`."""

    def hooked(self, answer: Any) -> Any:
        """Ответ через крючки сессии - так же, как это делает requests."""
        for hook in self.hooks['response']:
            hook(answer)
        return answer

    def __init__(self, version: int = metf.MIN_PROTOCOL, **wifi: object) -> None:
        self.version = version
        self.hooks: dict[str, list] = {'response': []}
        self.uptimes: list[int] = []
        self.wifi = {'state': 'online', 'mode': 'sta', 'connected': True,
                     'ssid': 'dav', 'source': 'build', 'rssi': -62,
                     'ap_up': False, 'problem': 'none', 'hw_error': False}
        self.wifi.update(wifi)
        self.gets: list[str] = []

    def get(self, url: str, timeout: float) -> FakeRead:
        self.gets.append(url)
        uptime = self.uptimes.pop(0) if self.uptimes else None
        if url.endswith('/version'):
            return self.hooked(FakeRead(text=str(self.version), uptime=uptime))
        if url.endswith('/wifi'):
            return self.hooked(FakeRead(payload=self.wifi, uptime=uptime))
        raise AssertionError(f'неожиданный запрос {url}')


class FakeRead:
    """Ответ на чтение: текст версии или JSON состояния."""

    def __init__(self, text: str = '', payload: dict | None = None,
                 uptime: int | None = None) -> None:
        self.text = text
        self._payload = payload
        self.headers = {} if uptime is None else {'X-Uptime-Ms': str(uptime)}

    def raise_for_status(self) -> None:
        return None

    def json(self) -> dict:
        assert self._payload is not None
        return self._payload


@pytest.fixture
def warnings(monkeypatch: pytest.MonkeyPatch) -> list[str]:
    """Предупреждения стенда списком: проверяем не только отказы, но и слова."""
    said: list[str] = []
    monkeypatch.setattr(metf.logger, 'warning', said.append)
    monkeypatch.setattr(metf.logger, 'info', lambda text: None)
    return said


def inspected(monkeypatch: pytest.MonkeyPatch, plate: FakeBoard,
              stand_ssid: str = 'waterius_stand', stand_channel: int = 0) -> str:
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    return metf.check(api, '192.0.2.1', stand_ssid, stand_channel)


def test_досмотр_валит_прошивку_младше_восьмого_протокола(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Старую плату надо ловить на старте, а не по кривым паузам между импульсами:
    до восьмого протокола ответ приходил после конца импульса и опаздывал на
    четверть секунды.
    """
    with pytest.raises(metf.MetfTooOld, match='протокол'):
        inspected(monkeypatch, FakeBoard(version=7))


def test_досмотр_валит_плату_на_точке_стенда(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Управляющий канал не должен лежать на том, что тесты ломают: первое же
    выключение точки уносит вместе с устройством и саму METF. Отказ обязан
    называть лекарство - иначе плату придётся доставать пробросом порта.
    """
    with pytest.raises(metf.MetfOnStandAp) as err:
        inspected(monkeypatch, FakeBoard(ssid='waterius_stand'))
    assert 'action=forget' in str(err.value)


def test_досмотр_пропускает_плату_в_домашней_сети(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """Здоровая плата не должна ни падать, ни жаловаться."""
    summary = inspected(monkeypatch, FakeBoard())
    assert f'протокол {metf.MIN_PROTOCOL}' in summary and 'обрывы: none' in summary
    assert warnings == []


def test_слабый_сигнал_остаётся_предупреждением(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Прогон на -78 дБм разваливался на сетевых тестах, но отказывать здесь
    нельзя: запас сигнала - повод предупредить, а не повод не пустить к железу.
    """
    summary = inspected(monkeypatch, FakeBoard(rssi=-78))
    assert '-78' in summary
    assert any('-78' in line for line in warnings), warnings


def test_запомненная_сеть_названа_предупреждением(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Запомненная порталом сеть перекрывает зашитую при сборке и переживает
    перезагрузку: после прерванного прогона плата вернётся не туда, где её
    ищет stand.ini.
    """
    inspected(monkeypatch, FakeBoard(source='saved'))
    assert any('forget' in line for line in warnings), warnings


def test_поднятая_точка_платы_видна_с_причиной(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """Своя точка METF означает потерю сети; причину протокол 10 говорит словами."""
    inspected(monkeypatch, FakeBoard(ap_up=True, problem='dropped'))
    assert any('dropped' in line for line in warnings), warnings


@pytest.mark.parametrize('version', [8, 11, 12])
def test_прошивка_младше_двенадцатой_к_прогону_не_допускается(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str],
        version: int) -> None:
    """
    До двенадцатого протокола чтение лога шло без подтверждения: не доехавший
    ответ уносил строки, и тест падал на неполном логе не там, где сломалось.
    Держать ради этого второй путь чтения дороже, чем прошить плату.
    """
    with pytest.raises(metf.MetfTooOld, match=f'протокол {version}'):
        inspected(monkeypatch, FakeBoard(version=version))


def test_перекрывающиеся_каналы_названы_причиной(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Точка стенда на 6, домашняя сеть на 4 - замер 23 сентября 2026. Так и было:
    сегменты гибли, тела посылок приходили обрезанными, чтение лога проваливалось
    на секунду. Стенд обязан называть это сам, иначе ищут дефект прошивки.
    """
    inspected(monkeypatch, FakeBoard(channel=4), stand_channel=6)
    assert any('перекрываются' in line for line in warnings), warnings


def test_разнесённые_каналы_молчат(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """1 и 6 не перекрываются - жаловаться не на что."""
    inspected(monkeypatch, FakeBoard(channel=1), stand_channel=6)
    assert warnings == []


def test_общий_канал_не_считается_бедой(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    На общем канале платы слышат друг друга и делят эфир по очереди. Это хуже
    разнесённых каналов по скорости, но не рвёт передачи, а ругаться на то, что
    само по себе не ломает прогон, - значит приучить не читать предупреждения.
    """
    inspected(monkeypatch, FakeBoard(channel=6), stand_channel=6)
    assert warnings == []


class SilentReader:
    """Плата, которая приняла `GET /read`, но ответа не прислала."""

    def __init__(self, error: Exception | None = None, answers: int = 0,
                 seq: int | None = None) -> None:
        self.error = error or requests.ReadTimeout('no answer')
        self.gets = 0
        self.acks: list[str] = []      # что стенд подтверждал в каждом запросе
        self.answers = answers         # сколько ответов отдать до молчания
        self.seq = seq                 # номер окна; None - плата протокола 11
        self.hooks: dict[str, list] = {'response': []}

    def get(self, url: str, params: dict | None = None,
            timeout: float = 0) -> FakeRead:
        self.gets += 1
        self.acks.append(str((params or {}).get('ack')))
        if self.answers > 0:
            self.answers -= 1
            answer = FakeRead(text='строка\n')
            if self.seq is not None:
                answer.headers['X-Log-Seq'] = str(self.seq)
            return answer
        raise self.error


def test_чтение_лога_повторяется_после_таймаута(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата придерживает отданное до подтверждения, поэтому не доехавший ответ
    ничего не стоит: повторить чтение можно и нужно.
    """
    plate = SilentReader()
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    with pytest.raises(requests.ReadTimeout):
        api.serial_read()
    assert plate.gets == 3, 'чтение лога обязано повторяться'


def test_уменьшившийся_аптайм_это_перезагрузка(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Аптайм меньше прошлого - плата стартовала заново. Отличить перезагрузку от
    занятости больше нечем, а последствия у неё заметные: кольцо лога пусто,
    сервер времени выключен, выводы вернулись во вход.
    """
    plate = FakeBoard()
    plate.uptimes = [90_000, 1_200]
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    mark = clock.now

    api.version()
    assert api.reboots == 0, 'первый ответ сравнивать не с чем'
    api.version()

    assert api.reboots == 1, 'перезагрузка не замечена'
    assert api.reboot_since(mark) is not None
    assert any('перезагрузилась' in line for line in warnings), warnings


def test_растущий_аптайм_молчит(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """Плата работает без перерыва - говорить не о чем."""
    plate = FakeBoard()
    plate.uptimes = [1_000, 2_000, 3_000]
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]

    for _ in range(3):
        api.version()

    assert api.reboots == 0
    assert api.reboot_since(0.0) is None
    assert warnings == []


def test_ответ_без_аптайма_не_ломает_клиента(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """
    Заголовка может не оказаться у отдельного ответа (ошибка, чужой прокси).
    Клиенту от этого плохеть нельзя: он просто не узнает о перезагрузке.
    """
    plate = FakeBoard()
    plate.uptimes = []              # ни один ответ аптайма не несёт
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]

    assert api.version() == metf.MIN_PROTOCOL
    assert api.reboots == 0 and api.uptime_ms is None


def test_перезагрузка_до_метки_не_считается_свежей(
        monkeypatch: pytest.MonkeyPatch, clock: Clock, warnings: list[str]) -> None:
    """`reboot_since` отвечает про окно теста, а не про весь прогон."""
    plate = FakeBoard()
    plate.uptimes = [90_000, 1_200]
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]
    api.version()
    api.version()

    assert api.reboot_since(clock.now + 1) is None


def test_подтверждение_делает_чтение_повторяемым(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Плата протокола 12 придерживает отданное до подтверждения, поэтому
    потерянный ответ ничего не стоит: тот же запрос отдаёт то же окно.
    """
    plate = SilentReader(answers=1, seq=7)
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]

    assert api.serial_read() == 'строка\n'
    assert plate.acks == ['0'], 'первое чтение ничего не подтверждает'

    with pytest.raises(requests.ReadTimeout):
        api.serial_read()
    assert plate.gets == 4, 'чтение с подтверждением обязано повторяться'
    assert plate.acks[1:] == ['7', '7', '7'], 'повтор просит то же окно'


def test_ответ_без_номера_окна_это_отказ(
        monkeypatch: pytest.MonkeyPatch, clock: Clock) -> None:
    """
    Досмотр перед прогоном требует протокол 12, так что номер окна обязан быть.
    Его отсутствие значит, что плату подменили на ходу, - молчать нельзя:
    подтверждать станет нечем, и строки начнут теряться.
    """
    plate = SilentReader(answers=1, seq=None)
    api = board(monkeypatch, FakeClient(failures=0, session=plate))  # type: ignore[arg-type]

    with pytest.raises(AssertionError, match='номер окна'):
        api.serial_read()
