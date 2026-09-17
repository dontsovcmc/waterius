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
