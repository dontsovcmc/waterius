"""
Приёмник посылок Ватериуса - и одновременно канал настройки устройства.

Успехом прошивка считает только код 200 (https_helpers.cpp:post_data), поэтому
приёмник обязан его возвращать; всё остальное превратится в SEND_BAD_ANSWER и
шесть вспышек светодиода.

Главное здесь - reply_settings(). Тело ответа, если это JSON-объект, прошивка
копирует в json_settings, применяет (apply_settings) и тут же отправляет данные
повторно (main.cpp). Значит из теста можно менять любую настройку - пороги
тревог, вес импульса, период, маску квитанции - без портала, без брокера и без
человека. В логе это видно строкой `Apply setting: <имя>=<значение>`, а
результат приезжает во второй посылке того же сеанса.

Приёмник умеет отвечать не только двумястами (`reply_status`) и слушать
вдобавок https на своём порту (`start_tls`). Первое нужно, чтобы проверить
`SEND_BAD_ANSWER`: успехом прошивка считает только 200, всё остальное - «сервер
ответил не то». Второе - чтобы проверить свой сервер по https: сертификат
самоподписанный, и это верный опыт, потому что прошивка его не проверяет
(`https_helpers.cpp`, setInsecure).
"""

from __future__ import annotations

import json
import queue
import ssl
import subprocess
import tempfile
import threading
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Iterator

from loguru import logger


def self_signed(host: str, directory: Path) -> tuple[Path, Path]:
    """
    Самоподписанный сертификат на адрес приёмника.

    Имя в сертификате - IP, а не доменное имя: устройство ходит на стенд по
    адресу. Проверять его никто не будет, но сертификат без SAN отвергают уже
    некоторые клиенты, а нам нужен опыт, а не заведомо кривой сертификат.
    """
    key = directory / 'receiver.key'
    cert = directory / 'receiver.crt'
    subprocess.run(
        ['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
         '-keyout', str(key), '-out', str(cert), '-days', '30',
         '-subj', f'/CN={host}', '-addext', f'subjectAltName=IP:{host}'],
        check=True, capture_output=True)
    return cert, key


class Receiver:
    """HTTP-приёмник в фоновом потоке."""

    def __init__(self, host: str = '0.0.0.0', port: int = 8000,
                 cert_host: str = '127.0.0.1') -> None:
        self.payloads: queue.Queue[dict[str, Any]] = queue.Queue()
        self.history: list[dict[str, Any]] = []
        self.tls_hits = 0
        self._files: dict[str, bytes] = {}
        self._reply: dict[str, Any] | None = None
        self._reply_once = True
        self._status = 200
        self._lock = threading.Lock()
        self._tls: ThreadingHTTPServer | None = None
        self._tls_thread: threading.Thread | None = None
        self._certs = tempfile.TemporaryDirectory(prefix='hil-cert-')
        # Слушаем на всех адресах, а в сертификате должен стоять тот, по
        # которому на стенд ходит устройство
        self.cert_host = cert_host

        receiver = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self) -> None:                    # noqa: N802
                length = int(self.headers.get('Content-Length', 0))
                raw = self.rfile.read(length)
                try:
                    payload = json.loads(raw)
                except json.JSONDecodeError:
                    logger.warning(f'приёмник: не JSON: {raw[:200]!r}')
                    payload = {}

                receiver._remember(payload, isinstance(self.connection, ssl.SSLSocket))
                status = receiver._take_status()
                body = receiver._take_reply() if status == 200 else b'{}'

                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self) -> None:                     # noqa: N802
                body = receiver.file(self.path)
                if body is None:
                    if self.path.rstrip('/') in ('', '/data'):
                        self.send_response(200)
                        self.end_headers()
                        self.wfile.write(b'pong')
                    else:
                        self.send_error(404)
                    return

                # Образ качает ESPhttpUpdate: ему нужна длина в заголовке,
                # по ней он решает, хватит ли места во флеше
                self.send_response(200)
                self.send_header('Content-Type', 'application/octet-stream')
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close')
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, fmt: str, *args: Any) -> None:
                pass                                       # свой лог у loguru

        self._handler = Handler
        self._host = host
        self._server = ThreadingHTTPServer((host, port), Handler)
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)

    def start(self) -> None:
        self._thread.start()
        logger.info(f'приёмник слушает {self._server.server_address}')

    def start_tls(self, port: int, host: str = '') -> None:
        """
        Тот же приёмник вдобавок по https: посылки попадают в ту же очередь.

        Второй сокет, а не замена первому: в одном тесте устройство ходит на
        https, во всех остальных - на http, и переподнимать приёмник между
        тестами значило бы терять очередь.
        """
        if self._tls is not None:
            return
        cert, key = self_signed(self.cert_host, Path(self._certs.name))
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        self._tls = ThreadingHTTPServer((host or self._host, port), self._handler)
        self._tls.socket = context.wrap_socket(self._tls.socket, server_side=True)
        self._tls_thread = threading.Thread(target=self._tls.serve_forever, daemon=True)
        self._tls_thread.start()
        logger.info(f'приёмник слушает https {self._tls.server_address}')

    def stop(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        if self._tls is not None:
            self._tls.shutdown()
            self._tls.server_close()
        self._certs.cleanup()

    # --- приём ---

    def _remember(self, payload: dict[str, Any], secure: bool = False) -> None:
        with self._lock:
            self.history.append(payload)
            if secure:
                # Единственный способ доказать, что посылка пришла именно по
                # https: в теле она та же самая
                self.tls_hits += 1
        self.payloads.put(payload)

    def wait_payload(self, timeout: float = 60.0) -> dict[str, Any] | None:
        try:
            return self.payloads.get(timeout=timeout)
        except queue.Empty:
            return None

    def drain(self) -> None:
        while not self.payloads.empty():
            self.payloads.get_nowait()

    # --- раздача файлов ---

    def serve(self, path: str, data: bytes) -> str:
        """
        Отдавать файл по этому пути. Возвращает адрес, который можно положить
        в команду обновления: OTA качает образы по https, обычный порт для
        этого не годится (`ota_update.cpp`, WiFiClientSecure).
        """
        with self._lock:
            self._files[path] = data
        return path

    def file(self, path: str) -> bytes | None:
        with self._lock:
            return self._files.get(path.split('?', 1)[0])

    def forget(self, path: str) -> None:
        with self._lock:
            self._files.pop(path, None)

    # --- ответ устройству ---

    @contextmanager
    def answering(self, status: int) -> Iterator[None]:
        """
        Отвечать на посылки этим кодом, пока не выйдем из блока.

        Успехом прошивка считает только 200; любой другой код - это
        SEND_BAD_ANSWER, то есть «сервер ответил не то». Настройки в теле при
        этом не отдаём: прошивка их и не прочитает.
        """
        with self._lock:
            self._status = status
        try:
            yield
        finally:
            with self._lock:
                self._status = 200

    def _take_status(self) -> int:
        with self._lock:
            return self._status

    # --- настройка устройства ---

    def reply_settings(self, settings: dict[str, Any], once: bool = True) -> None:
        """
        Положить настройки в ответ на следующую посылку.

        once=True по умолчанию: на повторную отправку того же сеанса надо
        ответить пустым телом, иначе прошивка увидит настройки снова и сеанс
        зациклится на применении.
        """
        with self._lock:
            self._reply = dict(settings)
            self._reply_once = once

    def _take_reply(self) -> bytes:
        with self._lock:
            if self._reply is None:
                return b'{}'
            body = json.dumps(self._reply).encode()
            if self._reply_once:
                self._reply = None
            return body
