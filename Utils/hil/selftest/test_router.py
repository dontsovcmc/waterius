"""
Сетевая консоль роутера: обрыв соединения - без железа.

Цена вопроса известна по прогону 20.09: консоль WT32-ETH01 закрыла сокет на
середине многочасового набора, и дальше каждая команда падала на
`BrokenPipeError` - ни один из оставшейся сотни тестов до прошивки не дошёл.
Здесь проверяется, что обрыв переживается: перед отправкой консоль поднимается
заново, а обрыв на середине ответа называется своим именем, но связь после него
рабочая.
"""

from __future__ import annotations

import socket
import threading

import pytest

from ..router import RouterError, TcpTransport

PASSWORD = 'secret'


class FakeConsole:
    """
    Консоль роутера в одном потоке: принимает соединение, съедает пароль и
    отвечает на каждую команду эхом. Умеет рвать соединение по требованию.
    """

    def __init__(self) -> None:
        self._srv = socket.socket()
        self._srv.bind(('127.0.0.1', 0))
        self._srv.listen(1)
        self.port: int = self._srv.getsockname()[1]
        self.lines: list[str] = []
        self.connections = 0
        self.drop_next = False          # оборвать, не ответив на команду
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()

    def _serve(self) -> None:
        while not self._stop.is_set():
            try:
                conn, _ = self._srv.accept()
            except OSError:
                return
            self.connections += 1
            with conn:
                self._handle(conn)

    def _handle(self, conn: socket.socket) -> None:
        """Одно соединение. Выход отсюда - только конец этого соединения."""
        conn.settimeout(0.5)
        rest = b''
        while not self._stop.is_set():
            try:
                data = conn.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            if not data:
                return
            rest += data
            while b'\n' in rest:
                raw, rest = rest.split(b'\n', 1)
                line = raw.decode().strip()
                if line == PASSWORD:
                    continue
                self.lines.append(line)
                if self.drop_next:
                    self.drop_next = False
                    conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                    b'\x01\x00\x00\x00\x00\x00\x00\x00')
                    return          # RST вместо ответа
                try:
                    conn.sendall(f'{line}\r\nesp32> '.encode())
                except OSError:
                    return

    def close_client(self) -> None:
        """Закрыть текущее соединение, оставив сервер на приёме."""
        self.drop_next = True

    def stop(self) -> None:
        self._stop.set()
        self._srv.close()


@pytest.fixture
def console() -> FakeConsole:
    fake = FakeConsole()
    yield fake
    fake.stop()


def test_команда_после_обрыва_уходит_на_поднятой_заново_консоли(
        console: FakeConsole) -> None:
    transport = TcpTransport('127.0.0.1', PASSWORD, port=console.port)

    console.close_client()
    with pytest.raises(RouterError):
        transport.write_line('show status')
        transport.read_idle(1.0, 0.2)

    # Связь восстановлена: следующая команда доходит и получает ответ.
    transport.drain()
    transport.write_line('show config')
    assert 'show config' in transport.read_idle(2.0, 0.2)
    assert console.connections >= 2, 'консоль не поднималась заново'
    transport.close()


def test_обрыв_на_середине_ответа_назван_своим_именем(
        console: FakeConsole) -> None:
    transport = TcpTransport('127.0.0.1', PASSWORD, port=console.port)

    console.close_client()
    with pytest.raises(RouterError, match='оборвалась'):
        transport.write_line('show acl')
        transport.read_idle(1.0, 0.2)

    transport.close()
