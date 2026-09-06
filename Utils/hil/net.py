"""
Сетевые сценарии на языке тестов.

Разница между «сети нет» и «сеть есть, а сервера нет» принципиальна: первое даёт
две вспышки светодиода и ни одной строки Alarm confirm, второе - три вспышки и
строку со статусами получателей. Ручной план проверял только первое, потому что
человек может лишь выдернуть роутер. Правила фильтра дают второе, а заодно
единственный способ проверить маску квитанции: облако молчит, брокер отвечает.

Все сценарии - контекстные менеджеры с гарантированным возвратом. Тест, упавший
с выключенной точкой доступа, иначе уронил бы весь прогон: Ватериус просто не
нашёл бы сеть.
"""

from __future__ import annotations

from contextlib import contextmanager
from typing import Iterator

from loguru import logger

from .router import NatRouter

HTTPS_PORT = 443
HTTP_PORT = 80


class Net:
    """Сеть стенда: точка доступа и фильтр трафика Ватериуса."""

    def __init__(self, router: NatRouter, dut_ip: str, broker_port: int = 1883,
                 receiver_port: int = 8000) -> None:
        self.router = router
        self.dut_ip = dut_ip
        self.broker_port = broker_port
        self.receiver_port = receiver_port

    @contextmanager
    def ap_off(self) -> Iterator[None]:
        """Сети нет вовсе. Ожидаем две вспышки: не подключился к точке доступа."""
        logger.info('сеть: гасим точку доступа')
        with self.router.ap_off():
            yield

    @contextmanager
    def internet_down(self) -> Iterator[None]:
        """Wi-Fi живёт, весь исходящий трафик устройства отброшен."""
        logger.info('сеть: режем весь трафик устройства')
        with self.router.blocked(self.dut_ip):
            yield

    @contextmanager
    def cloud_down(self) -> Iterator[None]:
        """
        Облако недоступно, брокер жив. Режем и 443, и порт приёмника: облачных
        получателей два - waterius.ru и свой сервер, - а вопрос «кому доклад
        обязан доехать» решается по каждому отдельно.
        """
        logger.info('сеть: режем облако, брокер оставляем')
        self.router.block_port(self.dut_ip, HTTPS_PORT)
        self.router.block_port(self.dut_ip, HTTP_PORT)
        self.router.block_port(self.dut_ip, self.receiver_port)
        try:
            yield
        finally:
            self.router.acl_clear()

    @contextmanager
    def mqtt_down(self) -> Iterator[None]:
        """Брокер недоступен, облако живо: зеркало предыдущего сценария."""
        logger.info('сеть: режем брокер, облако оставляем')
        with self.router.blocked(self.dut_ip, self.broker_port):
            yield

    @contextmanager
    def weak_signal(self, dbm: int = 2) -> Iterator[None]:
        """
        Слабый сигнал: проверка RSSI в посылке и поведения на границе связи.
        Прежнюю мощность читаем у роутера, а не помним: она могла быть не
        заводской.
        """
        was = self.router.config().get('tx_power', '78')
        self.router.set_tx_power(dbm)
        try:
            yield
        finally:
            self.router.set_tx_power(int(was))

    @contextmanager
    def channel(self, number: int) -> Iterator[None]:
        """
        Сменить канал точки доступа. Работает только на сборке с проводным
        аплинком: при аплинке по Wi-Fi канал точки задаёт роутер.
        """
        logger.info(f'сеть: канал {number}')
        with self.router.channel(number):
            yield

    @contextmanager
    def renamed(self, ssid: str, password: str) -> Iterator[None]:
        """Сеть с другим именем: устройство не должно к ней подключаться."""
        logger.info(f'сеть: переименование в {ssid}')
        with self.router.ssid(ssid, password):
            yield

    def wait_dut_online(self, mac: str, timeout: float = 120.0) -> bool:
        return self.router.wait_client(mac, timeout)
