"""
Плата NodeMCU с прошивкой ESP-AT: HTTP-клиент внутри сети портала Ватериуса.

Зачем отдельный клиент. В режиме настройки Ватериус поднимает свою точку доступа
на 192.168.4.1, и попасть в неё некому: рабочая машина занята домашней сетью, а
точка стенда (WT32-ETH01 в сборке с проводным аплинком) присоединяться к чужим
сетям не умеет - там WIFI_MODE_AP. Прошивка платы и её проверка - 02_nodemcu-at.md.

Три правила, без которых ничего не работает; каждое проверено на живой паре.

1. Запрос обязан уйти сразу после CONNECT. ESPAsyncWebServer ставит клиенту
   `setRxTimeout(3)` (WebServer.cpp:44) и рвёт соединение, если за три секунды не
   пришло ни байта. Поэтому между `AT+CIPSTART` и `AT+CIPSEND` не должно быть ни
   пауз, ни лишних чтений: сеанс `CONNECT / OK / CLOSED` - это не отказ платы, а
   наш собственный простой.

2. Приём только пассивный (`AT+CIPRECVMODE=1`). В активном режиме данные
   приезжают сами (`+IPD`) и перемешиваются с ответами на команды - разобрать
   такой поток нечем.

3. Полученные данные читаются по длине, а не по строкам: `+CIPRECVDATA:<n>,`
   и дальше ровно n сырых байт, среди которых бывают и `\r\n`, и `OK`.

Признак конца ответа - его собственная длина, а не `CLOSED`: ждать закрытия
дороже, уведомление приходит с задержкой, и на нём один запрос обходился в
40 секунд. Страницы с подстановкой (`send_page` с процессором) отдаются
`Transfer-Encoding: chunked` без `Content-Length` - для них концом служит
нулевой кусок.
"""

from __future__ import annotations

import re
import time
from dataclasses import dataclass, field

import serial
from loguru import logger

DEFAULT_BAUD = 115200
CHUNK = 1024                 # столько байт забираем за один AT+CIPRECVDATA

RE_STATE = re.compile(r'\+CWSTATE:(\d+)')
RE_IP = re.compile(r'\+CIPSTA:ip:"([\d.]+)"')
RE_RECVLEN = re.compile(r'\+CIPRECVLEN:(\d+)')
# Нулевой кусок в конце chunked-ответа. Длина куска пишется в поле фиксированной
# ширины и добивается пробелами, поэтому концом служит "0   \r\n\r\n".
RE_LAST_CHUNK = re.compile(rb'(?:^|\r\n)0[ \t]*\r\n\r\n$')

STATE_GOT_IP = 2             # AT+CWSTATE?: 2 - подключена и получила адрес


class AtError(Exception):
    pass


@dataclass
class Response:
    status: int
    headers: dict[str, str] = field(default_factory=dict)
    body: bytes = b''

    @property
    def text(self) -> str:
        return self.body.decode('utf-8', 'replace')

    @property
    def content_length(self) -> int | None:
        value = self.headers.get('content-length')
        return int(value) if value is not None else None


class AtBoard:
    """AT-плата как HTTP-клиент: присоединиться к сети и забрать страницу."""

    def __init__(self, port: str, baud: int = DEFAULT_BAUD) -> None:
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.buf = b''
        self._passive = False
        time.sleep(0.3)
        self.ser.reset_input_buffer()

    def close(self) -> None:
        self.ser.close()

    # --- поток ---

    def _pump(self) -> bytes:
        chunk = self.ser.read(4096)
        if chunk:
            self.buf += chunk
        return chunk

    def _drain(self) -> None:
        """
        Выбросить всё, что осталось от предыдущей команды.

        Иначе `_until` найдёт чужой `OK` и вернётся раньше, чем плата ответит на
        новую команду. Так терялся весь обмен после первого же запроса.
        """
        while self._pump():
            pass
        self.buf = b''

    def _until(self, markers: tuple[bytes, ...], timeout: float) -> bytes:
        deadline = time.time() + timeout
        while time.time() < deadline:
            if not self._pump():
                time.sleep(0.01)
            for marker in markers:
                i = self.buf.find(marker)
                if i >= 0:
                    out, self.buf = self.buf[:i + len(marker)], self.buf[i + len(marker):]
                    return out
        out, self.buf = self.buf, b''
        raise AtError(f'плата не ответила {markers}: {out[-200:]!r}')

    def _read_exact(self, size: int, timeout: float) -> bytes:
        deadline = time.time() + timeout
        while len(self.buf) < size:
            if time.time() > deadline:
                raise AtError(f'принято {len(self.buf)} из {size} байт')
            if not self._pump():
                time.sleep(0.005)
        out, self.buf = self.buf[:size], self.buf[size:]
        return out

    # --- команды ---

    def cmd(self, line: str, timeout: float = 10.0,
            markers: tuple[bytes, ...] = (b'OK\r\n', b'ERROR\r\n', b'FAIL\r\n')) -> str:
        self._drain()
        self.ser.write(line.encode() + b'\r\n')
        return self._until(markers, timeout).decode('utf-8', 'replace')

    def _ensure_passive(self) -> None:
        """
        Включить пассивный приём, если он ещё не включён.

        Настройка не переживает перезагрузку модуля даже при `AT+SYSSTORE=1`
        (проверено: после сброса `AT+CIPRECVMODE?` отвечает 0), а порт
        открывается с перезагрузкой. Забытая команда выглядит не ошибкой, а
        пустым ответом сервера: данные приезжают сами, как `+IPD`, и
        `AT+CIPRECVLEN?` показывает ноль.
        """
        if self._passive:
            return
        self.cmd('AT+CIPRECVMODE=1')
        self._passive = True

    def wait_ready(self, timeout: float = 30.0) -> bool:
        """
        Дождаться, пока плата снова в сети.

        Открытие порта дёргает DTR/RTS, а на NodeMCU это перезагрузка модуля:
        первые команды после `AtBoard(...)` приходятся на момент, когда сеть ещё
        не поднялась. Сеть плата помнит сама - переподключаться не нужно.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            m = RE_STATE.search(self.cmd('AT+CWSTATE?', timeout=5))
            if m and int(m.group(1)) == STATE_GOT_IP:
                self._ensure_passive()
                return True
            time.sleep(0.5)
        return False

    def join(self, ssid: str, password: str = '', timeout: float = 30.0) -> str:
        """Присоединиться к сети и вернуть свой адрес. Открытая сеть - пустой пароль."""
        self.cmd('AT+CWMODE=1')
        answer = self.cmd(f'AT+CWJAP="{ssid}","{password}"', timeout=timeout)
        if 'OK' not in answer:
            raise AtError(f'не удалось подключиться к {ssid}: {answer}')
        self._passive = False
        self._ensure_passive()
        return self.ip()

    def ip(self) -> str:
        m = RE_IP.search(self.cmd('AT+CIPSTA?'))
        if not m:
            raise AtError('плата не сообщила свой адрес')
        return m.group(1)

    # --- HTTP ---

    def get(self, path: str, host: str, timeout: float = 30.0) -> Response:
        return self.request('GET', path, host, timeout=timeout)

    def post(self, path: str, host: str, body: bytes = b'',
             timeout: float = 30.0) -> Response:
        """
        POST без тела: параметры портал принимает и из строки запроса.

        `_parseReqHead` разбирает query у любого метода (WebRequest.cpp:285), а
        `request->params()` не различает, откуда параметр приехал. Тело нужно
        было бы кодировать вторым способом ради того же результата.
        """
        return self.request('POST', path, host, body=body, timeout=timeout)

    def request(self, method: str, path: str, host: str, body: bytes = b'',
                timeout: float = 30.0) -> Response:
        self._ensure_passive()
        head = (f'{method} {path} HTTP/1.1\r\nHost: {host}\r\n'
                'User-Agent: waterius-hil\r\nConnection: close\r\n')
        if method != 'GET':
            head += ('Content-Type: application/x-www-form-urlencoded\r\n'
                     f'Content-Length: {len(body)}\r\n')
        request = head.encode() + b'\r\n' + body

        self._drain()
        self.ser.write(f'AT+CIPSTART="TCP","{host}",80\r\n'.encode())
        answer = self._until((b'OK\r\n', b'ERROR\r\n'), 10).decode('utf-8', 'replace')
        if 'OK' not in answer:
            raise AtError(f'нет соединения с {host}: {answer}')

        # Дальше - без задержек: сервер закрывает молчащего клиента через 3 с.
        self.ser.write(f'AT+CIPSEND={len(request)}\r\n'.encode())
        self._until((b'>',), 5)
        self.ser.write(request)
        self._until((b'SEND OK',), 10)

        raw = self._receive(timeout)
        try:
            self.cmd('AT+CIPCLOSE', timeout=5)
        except AtError:
            pass                                  # сервер обычно закрывает сам
        return _parse(raw)

    def _receive(self, timeout: float) -> bytes:
        raw = b''
        deadline = time.time() + timeout
        while time.time() < deadline:
            if _complete(raw):
                break
            answer = self.cmd('AT+CIPRECVLEN?', timeout=5)
            closed = 'CLOSED' in answer
            m = RE_RECVLEN.search(answer)
            available = int(m.group(1)) if m else 0
            if not available:
                if closed:
                    break
                time.sleep(0.05)
                continue

            self._drain()
            self.ser.write(f'AT+CIPRECVDATA={min(available, CHUNK)}\r\n'.encode())
            self._until((b'+CIPRECVDATA:',), 10)
            size = int(self._until((b',',), 5)[:-1])
            raw += self._read_exact(size, 10)
            self._until((b'OK\r\n',), 5)
            deadline = time.time() + timeout
        return raw


def _content_length(head: bytes) -> int | None:
    for line in head.split(b'\r\n')[1:]:
        name, _, value = line.partition(b':')
        if name.strip().lower() == b'content-length':
            return int(value.strip())
    return None


def _complete(raw: bytes) -> bool:
    """Ответ приехал целиком: по длине тела или по нулевому куску."""
    head, sep, body = raw.partition(b'\r\n\r\n')
    if not sep:
        return False
    length = _content_length(head)
    if length is not None:
        return len(body) >= length
    if _chunked(head):
        return RE_LAST_CHUNK.search(body) is not None
    return False


def _chunked(head: bytes) -> bool:
    for line in head.split(b'\r\n')[1:]:
        name, _, value = line.partition(b':')
        if name.strip().lower() == b'transfer-encoding':
            return b'chunked' in value.strip().lower()
    return False


def _dechunk(body: bytes) -> bytes:
    """Собрать тело из кусков. Куски видны только нам: сервер режет их сам."""
    out = b''
    rest = body
    while True:
        line, sep, rest = rest.partition(b'\r\n')
        if not sep:
            break
        size = int(line.split(b';')[0], 16)
        if size == 0:
            break
        out += rest[:size]
        rest = rest[size + 2:]                    # кусок и CRLF за ним
    return out


def _parse(raw: bytes) -> Response:
    head, _, body = raw.partition(b'\r\n\r\n')
    if not head.startswith(b'HTTP/'):
        raise AtError(f'это не ответ HTTP: {raw[:120]!r}')
    status = int(head.split(b' ')[1])
    if _chunked(head):
        body = _dechunk(body)
    headers = {}
    for line in head.split(b'\r\n')[1:]:
        name, sep, value = line.partition(b':')
        if sep:
            headers[name.strip().decode().lower()] = value.strip().decode()
    return Response(status=status, headers=headers, body=body)


if __name__ == '__main__':                        # ручная проверка платы
    import argparse

    parser = argparse.ArgumentParser(description='AT-плата: подключиться и забрать страницу')
    parser.add_argument('--port', default='/dev/cu.usbserial-0001')
    parser.add_argument('--ssid')
    parser.add_argument('--password', default='')
    parser.add_argument('--host', default='192.168.4.1')
    parser.add_argument('path', nargs='?', default='/')
    args = parser.parse_args()

    board = AtBoard(args.port)
    if args.ssid:
        logger.info(f'адрес платы: {board.join(args.ssid, args.password)}')
    else:
        board.wait_ready()
        logger.info(f'адрес платы: {board.ip()}')
    answer = board.get(args.path, args.host)
    logger.info(f'{answer.status}, {len(answer.body)} байт, '
                f'Content-Length={answer.content_length}')
    print(answer.text[:2000])
