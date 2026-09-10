"""
Сетевые сценарии на языке тестов.

Три несчастья различаются по-разному, и путать их нельзя: нет роутера - в логе
нет ни одной строки о доставке; нет облака - строка `Alarm confirm` есть, и в
ней `waterius=3`; нет своего сервера - та же строка, но `http=3`. Ручной план
проверял только первое, потому что человек может лишь выдернуть роутер.
Правила фильтра дают остальные, а заодно единственный способ проверить маску
квитанции: облако молчит, брокер отвечает.

Облако и свой сервер разводятся по портам: облако - это https на 443, свой
сервер стенда - порт приёмника. Резать по адресу не нужно.

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

# Список, в котором режется исходящий трафик клиента точки. Имя обманчиво:
# пакет клиента приходит на интерфейс точки, а обработчик входа сверяется с
# `to_ap` (netif_hooks.c). Здесь оно нужно, чтобы читать счётчики того же
# списка, в который NatRouter кладёт правила.
CLIENT_OUT = 'to_ap'


class Net:
    """Сеть стенда: точка доступа и фильтр трафика Ватериуса."""

    def __init__(self, router: NatRouter, dut_ip: str, dut_mac: str = '',
                 broker_port: int = 1883, receiver_port: int = 8000) -> None:
        self.router = router
        self.dut_ip = dut_ip
        self.dut_mac = dut_mac.lower()
        self.broker_port = broker_port
        self.receiver_port = receiver_port
        self._identity_checked = False

    def verify_dut(self) -> None:
        """
        Убедиться, что dut_ip закреплён именно за Ватериусом, прежде чем резать
        трафик.

        Правило фильтра, наведённое на чужой или пустой адрес, ничего не режет:
        посылка уходит, тест падает и обвиняет прошивку в том, что она не
        доставила данные.

        Проверяется резервирование, а не список подключённых: Ватериус спит
        почти всё время и в списке клиентов появляется на те полминуты, что
        длится сеанс, - а правила ставятся заранее. Если он всё-таки в сети,
        адрес сверяется и по факту.
        """
        if self._identity_checked:
            return
        assert self.dut_mac, (
            'в stand.ini не задан [dut] mac: без него адрес за Ватериусом не '
            'закреплён, и правила фильтра лягут на чужой адрес')

        reserved = self.router.dhcp_reservations()
        assert reserved.get(self.dut_mac) == self.dut_ip, (
            f'адрес {self.dut_ip} не закреплён за {self.dut_mac}: роутер держит '
            f'{reserved or "ни одного резервирования"}')

        seen = {c['mac']: c['ip'] for c in self.router.clients()}
        if self.dut_mac in seen:
            assert seen[self.dut_mac] == self.dut_ip, (
                f'Ватериус в сети, но с адресом {seen[self.dut_mac]}, а правила '
                f'ставятся на {self.dut_ip}: выдача прошла до резервирования, '
                'нужен новый сеанс')
        self._identity_checked = True

    @contextmanager
    def ap_off(self) -> Iterator[None]:
        """Сети нет вовсе. Ожидаем две вспышки: не подключился к точке доступа."""
        logger.info('сеть: гасим точку доступа')
        with self.router.ap_off():
            yield

    @contextmanager
    def _blocking(self, what: str) -> Iterator[None]:
        """
        Убедиться, что фильтр не просто завёлся, а действительно резал.

        Заведённое правило не значит ничего: с неверным списком оно так же
        читается в `show acl` и так же пропускает весь трафик. Именно так
        сценарии «сервер недоступен» полгода проходили, ничего не проверяя,
        и падали только тогда, когда до них добралось живое устройство.
        Поэтому концом сценария считается выросший счётчик отброшенных.
        """
        before = self.router.acl_stats(CLIENT_OUT).get('denied', 0)
        yield
        after = self.router.acl_stats(CLIENT_OUT).get('denied', 0)
        assert after > before, (
            f'{what}: фильтр не отбросил ни одного пакета '
            f'(denied {before} -> {after}). Правило есть, но трафик идёт мимо: '
            f'проверьте список ({CLIENT_OUT}) и адрес {self.dut_ip}')

    @contextmanager
    def internet_down(self) -> Iterator[None]:
        """Wi-Fi живёт, весь исходящий трафик устройства отброшен."""
        self.verify_dut()
        logger.info('сеть: режем весь трафик устройства')
        with self.router.blocked(self.dut_ip), self._blocking('internet_down'):
            yield

    @contextmanager
    def waterius_down(self) -> Iterator[None]:
        """
        Облако waterius.ru недоступно, свой сервер и брокер живы.

        Режем только https: свой сервер стенда слушает обычный http на своём
        порту, и под правило не попадает. В логе это `waterius=3 http=1`.
        """
        self.verify_dut()
        logger.info('сеть: режем облако waterius.ru, свой сервер оставляем')
        with self.router.blocked(self.dut_ip, HTTPS_PORT), \
                self._blocking('waterius_down'):
            yield

    @contextmanager
    def own_server_down(self) -> Iterator[None]:
        """
        Свой сервер недоступен, облако и брокер живы: зеркало предыдущего.

        Приёмник стенда при этом не получит посылку - это и есть признак, но
        проверять надо строку лога: пустой приёмник бывает и от оборванной
        сети, а тут сеть в порядке.
        """
        self.verify_dut()
        logger.info('сеть: режем свой сервер, облако оставляем')
        with self.router.blocked(self.dut_ip, self.receiver_port), \
                self._blocking('own_server_down'):
            yield

    @contextmanager
    def mqtt_down(self) -> Iterator[None]:
        """Брокер недоступен, облако живо: зеркало предыдущего сценария."""
        self.verify_dut()
        logger.info('сеть: режем брокер, облако оставляем')
        with self.router.blocked(self.dut_ip, self.broker_port), \
                self._blocking('mqtt_down'):
            yield
