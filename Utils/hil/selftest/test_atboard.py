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
