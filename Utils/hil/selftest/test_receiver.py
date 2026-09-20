"""
Приёмник: оборванная передача - без устройства.

В прогоне 20.09 тело посылки пришло обрезанным на середине. Приёмник разбирал
его как JSON, не разбирал и клал в очередь пустой словарь - тот уезжал в сеанс
наравне с настоящими посылками, становился последней, и `S4` падал на
`KeyError: 'period_min'`. Об обрыве не говорила ни одна строка отчёта.

Здесь проверяется обратное: нечитаемое тело посылкой не считается и
запоминается отдельно, чтобы стенд мог назвать причину.
"""

from __future__ import annotations

import socket

import pytest

from ..receiver import Receiver

BODY = b'{"delta0":0,"ch0":100.09,'          # обрезано на середине
FULL = b'{"delta0":0,"ch0":100.09}'


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


@pytest.fixture
def receiver() -> Receiver:
    device = Receiver(host='127.0.0.1', port=free_port())
    device.start()
    yield device
    device.stop()


def port_of(receiver: Receiver) -> int:
    return receiver._server.server_address[1]


def post(receiver: Receiver, body: bytes, length: int | None = None) -> None:
    """
    Отправить POST и закрыть соединение.

    `length` больше тела - это и есть обрыв: заголовок обещает больше байт, чем
    пришло, и чтение упирается в конец соединения.
    """
    declared = len(body) if length is None else length
    head = (f'POST /data HTTP/1.1\r\nHost: stand\r\n'
            f'Content-Type: application/json\r\n'
            f'Content-Length: {declared}\r\n\r\n').encode()
    with socket.create_connection(('127.0.0.1', port_of(receiver)), timeout=5.0) as sock:
        sock.sendall(head + body)
        sock.shutdown(socket.SHUT_WR)
        try:
            sock.recv(4096)
        except OSError:
            pass


def test_оборванное_тело_не_считается_посылкой(receiver: Receiver) -> None:
    post(receiver, BODY, length=len(BODY) + 200)

    assert receiver.wait_payload(timeout=1.0) is None, (
        'обрывок уехал в очередь как посылка')
    broken = receiver.take_broken()
    assert len(broken) == 1, f'обрыв не запомнен: {broken}'
    assert broken[0].startswith(b'{"delta0"')


def test_забранный_обрыв_второй_раз_не_приходит(receiver: Receiver) -> None:
    post(receiver, BODY, length=len(BODY) + 200)

    assert len(receiver.take_broken()) == 1
    assert receiver.take_broken() == [], 'обрыв остался и портит следующий тест'


def test_целое_тело_остаётся_посылкой(receiver: Receiver) -> None:
    post(receiver, FULL)

    assert receiver.wait_payload(timeout=2.0) == {'delta0': 0, 'ch0': 100.09}
    assert receiver.take_broken() == []
