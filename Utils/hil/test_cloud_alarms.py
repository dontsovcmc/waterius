"""
Тревоги сквозь железо: устройство -> бэкенд -> кнопка -> устройство.

Единственное место, где проверяется стык прошивки и сервера. Всё остальное
тестируется по отдельности: прошивка стендом, сервер своими тестами, — и про
стык оба врут одинаково уверенно.

Устройству прописывается наш бэкенд как облако (``waterius_host``), а стендовый
приёмник остаётся на «своём сервере»: через него стенд по-прежнему настраивает
устройство и видит посылки. Команда снятия ``arst`` доедет от бэкенда тем же
путём, что и от приёмника: прошивка применяет настройки из ответа любого
получателя (``ESP8266/src/senders/send_data.cpp``).

Адрес бэкенда и токен владельца устройства — секция ``[cloud]`` в ``stand.ini``.
Без неё тесты пропускаются. **Только локальный бэкенд или dev2**: тест меняет
состояние устройства в базе.

Блок Y. Запуск: ``pytest --stand Utils/hil/test_cloud_alarms.py``
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING, Any

import pytest
import requests

from .constants import (ALARM_WAIT_S, LEAKAGE, PLANNED_PERIOD_MIN,
                        PLANNED_WAIT_S)
from .logwatch import ALARM_MODE, TRANSMIT_MODE

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .stand import Stand      # а сбор тестов должен работать без них

pytestmark = pytest.mark.stand

# Виды тревог на стороне сервера: зеркало ALARM_* из api/api.py бэкенда
SERVER_ALARM_WET = 2

# Сколько ждём, пока сервер разберёт посылку: приём асинхронный, задача
# уведомления и запись строки идут отдельной очередью
SERVER_WAIT_S = 30.0

HTTP_TIMEOUT = 10.0


class Cloud:
    """
    Тонкий клиент бэкенда: ровно то, что нужно тесту.
    """

    def __init__(self, url: str, token: str, device_key: str) -> None:
        self.url = url
        self.device_key = device_key
        self.headers = {'Authorization': f'Token {token}',
                        'Content-Type': 'application/json'}

    def source(self) -> dict[str, Any]:
        """
        Устройство стенда глазами сервера.

        :return: Словарь источника со списком каналов.
        :raises AssertionError: Устройства с таким ключом у владельца нет.
        """
        r = requests.get(f'{self.url}/api/source/', headers=self.headers,
                         timeout=HTTP_TIMEOUT)
        r.raise_for_status()

        # Наружу ключ отдаётся последними четырьмя символами
        tail = self.device_key[-4:]
        for source in r.json()['results']:
            if source['key'] == tail:
                return source

        raise AssertionError(f'устройства с ключом ...{tail} нет у владельца токена')

    def alarms(self, number: int | None = None) -> list[dict[str, Any]]:
        """
        Открытые тревоги устройства.

        :param number: Номер входа; None — все входы.
        :return: Список тревог с полями сервера.
        """
        out = []
        for channel in self.source()['channels']:
            if number is not None and channel['number'] != number:
                continue
            for alarm in channel.get('alarms') or []:
                out.append(dict(alarm, channel_id=channel['id'],
                                number=channel['number']))
        return out

    def wait_alarm(self, kind: int, number: int, timeout: float = SERVER_WAIT_S) -> dict:
        """
        Дождаться, пока сервер заведёт строку тревоги.

        :param kind: Вид тревоги на сервере.
        :param number: Номер входа.
        :param timeout: Сколько ждать, секунд.
        :return: Тревога.
        :raises AssertionError: Строка не появилась за отведённое время.
        """
        deadline = time.monotonic() + timeout
        seen: list[dict[str, Any]] = []
        while time.monotonic() < deadline:
            seen = self.alarms(number)
            for alarm in seen:
                if alarm['kind'] == kind:
                    return alarm
            time.sleep(2.0)

        raise AssertionError(f'сервер не завёл тревогу kind={kind} на входе {number} '
                             f'за {timeout:.0f} с; открытые тревоги: {seen}')

    def wait_cleared(self, kind: int, number: int, timeout: float) -> None:
        """
        Дождаться, пока тревога исчезнет из открытых.

        :param kind: Вид тревоги.
        :param number: Номер входа.
        :param timeout: Сколько ждать, секунд.
        :raises AssertionError: Тревога всё ещё открыта.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if not any(a['kind'] == kind for a in self.alarms(number)):
                return
            time.sleep(2.0)

        raise AssertionError(f'тревога kind={kind} на входе {number} осталась открытой')

    def clear(self, alarm: dict[str, Any]) -> requests.Response:
        """
        Нажать «Снять тревогу» так, как это делает кабинет.

        :param alarm: Тревога из :meth:`alarms`.
        :return: Ответ сервера.
        """
        return requests.post(
            f"{self.url}/api/channel/{alarm['channel_id']}/alarm/{alarm['id']}/clear/",
            headers=self.headers, timeout=HTTP_TIMEOUT)


@pytest.fixture
def cloud(cfg: Any) -> Cloud:
    """
    Клиент бэкенда. Без секции ``[cloud]`` в ``stand.ini`` тест пропускается.

    :return: Клиент.
    """
    if not (cfg.cloud_url and cfg.cloud_token and cfg.cloud_device_key):
        pytest.skip('секция [cloud] в stand.ini не заполнена')
    return Cloud(cfg.cloud_url, cfg.cloud_token, cfg.cloud_device_key)


@pytest.fixture
def on_cloud(stand: Stand, cfg: Any) -> Any:
    """
    Устройство шлёт показания и в наш бэкенд, и в стендовый приёмник.

    Приёмник остаётся: через него стенд настраивает устройство и наблюдает
    сеансы. Облако добавляется вторым получателем.

    :return: Сеанс настройки.
    """
    return stand.setup(waterius_on=1, waterius_host=cfg.cloud_data_url)


def test_Y1_wet_alarm_reaches_backend(stand: Stand, cloud: Cloud, on_cloud: Any,
                                      quiet: None) -> None:
    """Сработавший датчик протечки доезжает до базы сервера."""
    stand.setup(channel=1, ctype=LEAKAGE)
    stand.reset_observers()

    try:
        stand.dut.wet(channel=1, closed=True)
        stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)

        alarm = cloud.wait_alarm(SERVER_ALARM_WET, number=1)
        assert alarm['can_clear'] is True
        assert 'датчик протечки' in alarm['text']
    finally:
        stand.dut.wet(channel=1, closed=False)


@pytest.mark.slow
def test_Y2_no_duplicates_on_repeat(stand: Stand, cloud: Cloud, on_cloud: Any,
                                    quiet: None) -> None:
    """
    Тревога едет в каждой посылке, а строка на сервере остаётся одна.

    Наблюдаем плановыми сеансами, а не кнопкой: короткое нажатие снимает тревоги
    attiny, и тест проверил бы собственное нажатие.
    """
    stand.setup(channel=1, ctype=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
    stand.reset_observers()

    try:
        stand.dut.wet(channel=1, closed=True)
        stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
        first = cloud.wait_alarm(SERVER_ALARM_WET, number=1)

        stand.wait_session(timeout=PLANNED_WAIT_S, mode=TRANSMIT_MODE)

        wet = [a for a in cloud.alarms(number=1) if a['kind'] == SERVER_ALARM_WET]
        assert len(wet) == 1, f'дубль строки тревоги: {wet}'
        assert wet[0]['id'] == first['id'], 'строку пересоздали вместо того, чтобы оставить'
    finally:
        stand.dut.wet(channel=1, closed=False)


@pytest.mark.slow
def test_Y3_clear_from_server_reaches_device(stand: Stand, cloud: Cloud, on_cloud: Any,
                                             quiet: None) -> None:
    """
    Снятие из кабинета доезжает до железа и гасит тревогу.

    Это и есть проверка стыка: маску кладёт сервер, применяет прошивка, а
    закрывает строку снова сервер — по следующей посылке устройства.

    Ждём плановый сеанс: кнопка снимает тревоги attiny сама, и тест был бы
    зелёным при любой прошивке.
    """
    stand.setup(channel=1, ctype=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
    stand.reset_observers()

    stand.dut.wet(channel=1, closed=True)
    stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)
    alarm = cloud.wait_alarm(SERVER_ALARM_WET, number=1)

    # Датчик отпускаем: пока вход замкнут, прошивка поднимет тревогу заново
    stand.dut.wet(channel=1, closed=False)

    response = cloud.clear(alarm)
    assert response.status_code == 200, response.text

    stand.reset_observers()
    session = stand.wait_session(timeout=PLANNED_WAIT_S, mode=TRANSMIT_MODE)
    assert 'Apply setting: arst' in session.text, (
        f'маска снятия не доехала до устройства\n{session.text}')

    cloud.wait_cleared(SERVER_ALARM_WET, number=1, timeout=SERVER_WAIT_S)


@pytest.mark.slow
def test_Y4_two_alarms_clear_by_or_mask(stand: Stand, cloud: Cloud, on_cloud: Any,
                                        quiet: None) -> None:
    """
    Две тревоги снимаются сложенной маской, а не затирают друг друга.

    Через ``dict.update`` вторая команда затирала бы первую, и одна тревога
    оставалась бы висеть на устройстве — по серверу это не видно никак.
    """
    stand.setup(channel=0, ctype=LEAKAGE)
    stand.setup(channel=1, ctype=LEAKAGE, period_min=PLANNED_PERIOD_MIN)
    stand.reset_observers()

    try:
        stand.dut.wet(channel=0, closed=True)
        stand.dut.wet(channel=1, closed=True)
        stand.wait_session(timeout=ALARM_WAIT_S, mode=ALARM_MODE)

        first = cloud.wait_alarm(SERVER_ALARM_WET, number=0)
        second = cloud.wait_alarm(SERVER_ALARM_WET, number=1)

        stand.dut.wet(channel=0, closed=False)
        stand.dut.wet(channel=1, closed=False)

        assert cloud.clear(first).status_code == 200
        assert cloud.clear(second).status_code == 200

        stand.reset_observers()
        stand.wait_session(timeout=PLANNED_WAIT_S, mode=TRANSMIT_MODE)

        cloud.wait_cleared(SERVER_ALARM_WET, number=0, timeout=SERVER_WAIT_S)
        cloud.wait_cleared(SERVER_ALARM_WET, number=1, timeout=SERVER_WAIT_S)
    finally:
        stand.dut.wet(channel=0, closed=False)
        stand.dut.wet(channel=1, closed=False)


def test_Y5_leak_sensor_channel_has_no_readings(stand: Stand, cloud: Cloud,
                                                on_cloud: Any, quiet: None) -> None:
    """У входа с датчиком протечки сервер не заводит показаний."""
    stand.setup(channel=1, ctype=LEAKAGE)
    stand.reset_observers()

    stand.dut.press_button()
    stand.wait_session(timeout=ALARM_WAIT_S)

    channel = next(c for c in cloud.source()['channels'] if c['number'] == 1)
    assert channel['is_leak_sensor'] is True
    assert channel['counts_impulses'] is False
    assert channel['is_work'] is True, 'карточка датчика не должна исчезать из кабинета'
