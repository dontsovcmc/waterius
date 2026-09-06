"""
MQTT-брокер на время прогона.

Стенд не должен зависеть от чужого брокера: тест, проверяющий retain или
автодискавери, обязан начинаться с чистых топиков. Поднимаем mosquitto на
сгенерированном конфиге и гасим в конце.

persistence выключена намеренно. Проверка «удерживаемое сообщение переживает
перезапуск брокера» - это проверка mosquitto, а не прошивки; вместо неё тест
подключается новым подписчиком и смотрит флаг retain в пришедшем сообщении.
"""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from loguru import logger

CONFIG = """\
listener {port}
allow_anonymous true
persistence false
"""


class Mosquitto:
    """Локальный брокер. Если бинарника нет, тесты MQTT надо пропускать."""

    def __init__(self, port: int = 1883) -> None:
        self.port = port
        self._proc: subprocess.Popen[bytes] | None = None
        self._dir: tempfile.TemporaryDirectory[str] | None = None

    @staticmethod
    def available() -> bool:
        return shutil.which('mosquitto') is not None

    def start(self, wait: float = 2.0) -> None:
        if not self.available():
            raise RuntimeError('mosquitto не найден: brew install mosquitto')

        self._dir = tempfile.TemporaryDirectory(prefix='hil-mosquitto-')
        conf = Path(self._dir.name) / 'mosquitto.conf'
        conf.write_text(CONFIG.format(port=self.port), encoding='utf-8')

        self._proc = subprocess.Popen(
            ['mosquitto', '-c', str(conf)],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        time.sleep(wait)

        if self._proc.poll() is not None:
            err = self._proc.stderr.read().decode() if self._proc.stderr else ''
            raise RuntimeError(f'mosquitto не поднялся: {err.strip()}')

        logger.info(f'брокер слушает порт {self.port}')

    def stop(self) -> None:
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()
        if self._dir:
            self._dir.cleanup()
