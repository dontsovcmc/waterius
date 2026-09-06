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

TLS нет намеренно: свой сервер на стенде обычный http.
"""

from __future__ import annotations

import json
import queue
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

from loguru import logger


class Receiver:
    """HTTP-приёмник в фоновом потоке."""

    def __init__(self, host: str = '0.0.0.0', port: int = 8000) -> None:
        self.payloads: queue.Queue[dict[str, Any]] = queue.Queue()
        self.history: list[dict[str, Any]] = []
        self._reply: dict[str, Any] | None = None
        self._reply_once = True
        self._lock = threading.Lock()

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

                receiver._remember(payload)
                body = receiver._take_reply()

                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self) -> None:                     # noqa: N802
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'pong')

            def log_message(self, fmt: str, *args: Any) -> None:
                pass                                       # свой лог у loguru

        self._server = ThreadingHTTPServer((host, port), Handler)
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)

    def start(self) -> None:
        self._thread.start()
        logger.info(f'приёмник слушает {self._server.server_address}')

    def stop(self) -> None:
        self._server.shutdown()
        self._server.server_close()

    # --- приём ---

    def _remember(self, payload: dict[str, Any]) -> None:
        with self._lock:
            self.history.append(payload)
        self.payloads.put(payload)

    def wait_payload(self, timeout: float = 60.0) -> dict[str, Any] | None:
        try:
            return self.payloads.get(timeout=timeout)
        except queue.Empty:
            return None

    def drain(self) -> None:
        while not self.payloads.empty():
            self.payloads.get_nowait()

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
