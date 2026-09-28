"""
Драйвер AT-платы: что он говорит плате на прощание. Железо не нужно.

Проверка живёт здесь, а не на стенде, потому что на стенде дефект невидим:
портальные тесты проходят одинаково и с погашенным радио, и с оставленным
включённым. Видно его только по нагреву платы через сутки.
"""

from __future__ import annotations

import pytest
import serial

from .. import atboard


class FakeSerial:
    """Плата, отвечающая `OK` на любую команду и помнящая, что ей писали."""

    def __init__(self) -> None:
        self.written: list[bytes] = []
        self.closed = False
        self._pending = b''

    def read(self, size: int = 1) -> bytes:
        out, self._pending = self._pending[:size], self._pending[size:]
        return out

    def write(self, data: bytes) -> int:
        self.written.append(data)
        self._pending += b'OK\r\n'
        return len(data)

    def reset_input_buffer(self) -> None:
        self._pending = b''

    def close(self) -> None:
        self.closed = True

    @property
    def sent(self) -> bytes:
        return b''.join(self.written)


@pytest.fixture
def board(monkeypatch: pytest.MonkeyPatch) -> atboard.AtBoard:
    fake = FakeSerial()
    monkeypatch.setattr(atboard.serial, 'Serial', lambda *a, **kw: fake)
    monkeypatch.setattr(atboard.time, 'sleep', lambda _: None)
    return atboard.AtBoard('/dev/fake')


def test_close_гасит_радио(board: atboard.AtBoard) -> None:
    """
    Точка портала исчезает вместе с режимом настройки, а плата, брошенная
    станцией, ищет её вечно (CWAUTOCONN=1, CWRECONNCFG - раз в секунду без
    предела) и греется. Уходя, гасим радио.
    """
    board.close()
    sent = board.ser.sent
    assert b'AT+CWQAP' in sent, 'плата осталась присоединённой к точке портала'
    assert b'AT+CWMODE=0' in sent, 'радио осталось включённым'
    assert board.ser.closed


def test_close_закрывает_порт_даже_если_плата_не_отвечает(
        board: atboard.AtBoard) -> None:
    """Молчащая или выдернутая плата не должна оставлять порт открытым."""
    def dead(_: bytes) -> int:
        raise serial.SerialException('порт отвалился')

    board.ser.write = dead                      # type: ignore[method-assign]
    board.close()
    assert board.ser.closed


def test_wait_ready_включает_радио_обратно(board: atboard.AtBoard) -> None:
    """
    `close()` гасит радио, а выключенное оно переживает перезагрузку модуля
    (`AT+SYSSTORE` по умолчанию 1). Ручной путь без `join()` обязан его вернуть.
    """
    board.wait_ready(timeout=0.01)
    assert b'AT+CWMODE=1' in board.ser.sent


class ScriptedSerial(FakeSerial):
    """
    Плата, отвечающая по сценарию: для каждой команды - своя очередь ответов,
    последний повторяется. Нужна там, где ответ `OK` на всё скрывает проверку:
    возвращение в сеть видно только по разным ответам на одну и ту же команду.
    """

    def __init__(self, script: dict[bytes, list[bytes]]) -> None:
        super().__init__()
        self.script = script

    def write(self, data: bytes) -> int:
        self.written.append(data)
        for prefix, answers in self.script.items():
            if data.startswith(prefix):
                self._pending += answers[0] if len(answers) == 1 else answers.pop(0)
                return len(data)
        self._pending += b'OK\r\n'
        return len(data)

    def count(self, prefix: bytes) -> int:
        return sum(1 for line in self.written if line.startswith(prefix))


def scripted(monkeypatch: pytest.MonkeyPatch,
             script: dict[bytes, list[bytes]]) -> atboard.AtBoard:
    fake = ScriptedSerial(script)
    monkeypatch.setattr(atboard.serial, 'Serial', lambda *a, **kw: fake)
    monkeypatch.setattr(atboard.time, 'sleep', lambda _: None)
    return atboard.AtBoard('/dev/fake')


# Ответ сервера целиком: `_complete` считает его законченным по Content-Length.
PAGE = b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi'


def портальный_сценарий(starts: list[bytes], state: bytes) -> dict[bytes, list[bytes]]:
    return {
        b'AT+CIPSTART': starts,
        b'AT+CWSTATE?': [b'+CWSTATE:' + state + b',"waterius"\r\n\r\nOK\r\n'],
        b'AT+CIPSTA?': [b'+CIPSTA:ip:"192.168.4.2"\r\n\r\nOK\r\n'],
        b'AT+CIPSEND': [b'>'],
        b'AT+CIPRECVLEN?': [b'+CIPRECVLEN:' + str(len(PAGE)).encode() + b'\r\n\r\nOK\r\n'],
        b'AT+CIPRECVDATA': [b'+CIPRECVDATA:' + str(len(PAGE)).encode() + b',' + PAGE + b'\r\nOK\r\n'],
        b'GET ': [b'SEND OK\r\n'],               # сам запрос уходит сырым, без AT
    }


def test_потерянная_точка_возвращается_и_запрос_доходит(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Прогон 20.09: на семнадцатой странице `AT+CIPSTART` ответил `ERROR`, и все
    оставшиеся тесты блока упали одинаково, ни разу не попробовав вернуться.
    Станция вне сети (`CWSTATE` не 2) - это `AT+CWJAP` и повтор.
    """
    board = scripted(monkeypatch, портальный_сценарий(
        [b'ERROR\r\n', b'OK\r\n'], state=b'1'))
    board.ssid = 'waterius-портал'

    answer = board.get('/', '192.168.4.1')

    assert answer.status == 200 and answer.body == b'hi'
    assert board.ser.count(b'AT+CWJAP') == 1, 'плата не вернулась в сеть'
    assert board.ser.count(b'AT+CIPSTART') == 2, 'запрос не повторён'
    # Возврат молчаливый: без счётчика тест, проверяющий «связь не рвалась»,
    # принял бы починенный обрыв за его отсутствие
    assert board.rejoins == 1, 'молчаливый возврат в сеть не посчитан'


def test_занятое_соединение_закрывается_а_не_переподключается(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Адрес есть (`CWSTATE` = 2), а соединение не открылось - занято прошлым
    запросом. Тогда возвращаться некуда: надо закрыть старое.
    """
    board = scripted(monkeypatch, портальный_сценарий(
        [b'ERROR\r\n', b'OK\r\n'], state=b'2'))
    board.ssid = 'waterius-портал'

    assert board.get('/', '192.168.4.1').status == 200
    assert board.ser.count(b'AT+CIPCLOSE') >= 1, 'висящее соединение не закрыто'
    assert board.ser.count(b'AT+CWJAP') == 0, 'зря переподключились: адрес был'


def test_вторая_осечка_называет_состояние_платы(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Если и повтор не удался, в ошибке обязано стоять состояние платы: без него
    отчёт молчит о том, что станция вне сети.
    """
    board = scripted(monkeypatch, портальный_сценарий(
        [b'ERROR\r\n'], state=b'1'))
    board.ssid = None                            # точка неизвестна

    with pytest.raises(atboard.AtError, match='CWSTATE=1'):
        board.get('/', '192.168.4.1')


def test_join_запоминает_сеть(monkeypatch: pytest.MonkeyPatch) -> None:
    """Возвращаться после обрыва плате некуда, если она не помнит, где была."""
    board = scripted(monkeypatch, портальный_сценарий([b'OK\r\n'], state=b'2'))
    assert board.join('waterius-портал') == '192.168.4.2'
    assert board.ssid == 'waterius-портал'


# Ответ живой платы на AT+CWLAP, снятый со стенда 28.09: своя сеть, скрытая
# соседская и точка стенда. Хвост полей зависит от AT+CWLAPOPT и не разбирается
LAP = (b'AT+CWLAP\r\n'
       b'+CWLAP:(3,"dav",-60,"7c:52:59:3f:23:9b",4,-1,-1,4,4,7,1)\r\n'
       b'+CWLAP:(0,"",-64,"46:df:65:bb:23:d9",2,-1,-1,0,0,7,0)\r\n'
       b'+CWLAP:(3,"waterius_stand",-87,"1C:C3:AB:3A:D3:11",11,-1,-1,4,4,7,0)\r\n'
       b'\r\nOK\r\n')


def test_скан_читает_канал_чужой_точки(monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Канал точки портала стенду взять больше неоткуда: прошивка о переезде
    молчит, а войти в точку, чтобы спросить, - значит потерять сам переезд.
    """
    board = scripted(monkeypatch, {b'AT+CWLAP': [LAP]})

    networks = board.scan()

    assert [(net.ssid, net.channel) for net in networks] == [
        ('dav', 4), ('', 2), ('waterius_stand', 11)]
    наша = networks[2]
    assert наша.rssi == -87
    assert наша.bssid == '1c:c3:ab:3a:d3:11', 'BSSID сравнивают с ответом портала, он в нижнем регистре'


def test_скан_по_имени_не_сканирует_весь_эфир(monkeypatch: pytest.MonkeyPatch) -> None:
    """Полный скан идёт секунды; когда ищут одну точку, спрашивают одну."""
    board = scripted(monkeypatch, {b'AT+CWLAP': [LAP]})

    board.scan('waterius_stand')

    assert board.ser.sent.startswith(b'AT+CWLAP="waterius_stand"')


def test_скан_без_ответа_платы_это_ошибка(monkeypatch: pytest.MonkeyPatch) -> None:
    """Пустой список и отказ платы - разные новости, путать их нельзя."""
    board = scripted(monkeypatch, {b'AT+CWLAP': [b'ERROR\r\n']})

    with pytest.raises(atboard.AtError, match='скан эфира'):
        board.scan()


def test_wait_offline_ждёт_пока_точка_уведёт_станцию(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Станция в сети - переезд ещё не начался. Ждём, а не спрашиваем один раз:
    ЕСП уходит на канал роутера не мгновенно.
    """
    board = scripted(monkeypatch, {b'AT+CWSTATE?': [
        b'+CWSTATE:2,"waterius"\r\n\r\nOK\r\n',
        b'+CWSTATE:2,"waterius"\r\n\r\nOK\r\n',
        b'+CWSTATE:4,"waterius"\r\n\r\nOK\r\n']})

    assert board.wait_offline(timeout=10) is True
    assert board.ser.count(b'AT+CWSTATE?') == 3


def test_wait_offline_не_висит_дольше_потолка(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """Станция, оставшаяся в сети, - это отказ теста, а не вечное ожидание."""
    board = scripted(monkeypatch, {b'AT+CWSTATE?': [
        b'+CWSTATE:2,"waterius"\r\n\r\nOK\r\n']})

    assert board.wait_offline(timeout=0.2) is False
