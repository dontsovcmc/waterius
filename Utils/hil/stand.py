"""
Стенд целиком: плата-манипулятор, точка доступа, приёмник, брокер.

Тест должен читаться как сценарий, а не как разбор лога, поэтому здесь три
вещи. Первая - setup(): любая настройка устройства одной строкой, без портала.
Вторая - wait_session(): сеанс приезжает одним объектом, в котором уже сведены
лог, посылка и сообщения брокера. Третья - net: сетевые сценарии контекстными
менеджерами.

Почему настройка идёт через ответ приёмника, а не через MQTT: ответ гарантированно
приходит в том же сеансе, в котором отправлены данные, и его не надо снимать с
retain. MQTT-путь проверяется отдельными тестами - он сам по себе функция.
"""

from __future__ import annotations

import time
from typing import Any

from loguru import logger
from metf_python_client import METFClient

from .config import StandConfig
from .dut import Dut
from .logwatch import LogWatcher, Session
from .mqttwatch import MqttWatch
from .net import Net
from .receiver import Receiver
from .router import NatRouter, connect

# Понятные имена вместо параметров прошивки. Хранятся здесь, а не в тестах,
# чтобы при переименовании параметра правка была одна.
CHANNEL_PARAMS = {
    'factor': 'f',
    'alarm_flow': 'af',
    'alarm_leak': 'al',
    'alarm_stop': 'as',
    'ctype': 'ctype',
    'serial': 'serial',
    'cname': 'cname',
    'value': 'ch',
}

GLOBAL_PARAMS = {
    'vacation': 'vac',
    'period_min': 'period_min',
    'send_on_consumption': 'sc',
    'confirm_waterius': 'ackw',
    'confirm_http': 'ackh',
    'confirm_mqtt': 'ackm',
}


class Stand:
    """Фасад над всем железом стенда."""

    def __init__(self, cfg: StandConfig, api: METFClient, router: NatRouter,
                 receiver: Receiver, mqtt: MqttWatch | None) -> None:
        self.cfg = cfg
        self.api = api
        self.router = router
        self.receiver = receiver
        self.mqtt = mqtt
        self.log = LogWatcher(api)
        self.dut = Dut(api, cfg.button_pin, cfg.ch0_pin, cfg.ch1_pin, cfg.reset_pin)
        self.net = Net(router, cfg.dut_ip, cfg.broker_port, cfg.receiver_port)

    # --- жизненный цикл ---

    @classmethod
    def create(cls, cfg: StandConfig, mqtt: MqttWatch | None = None) -> 'Stand':
        api = METFClient(cfg.metf_host)
        api.ping()
        api.serial_begin()
        router = connect(cfg.router_port or None, cfg.router_host or None,
                         cfg.router_password)
        receiver = Receiver(port=cfg.receiver_port)
        receiver.start()

        stand = cls(cfg, api, router, receiver, mqtt)
        stand.dut.init()

        # Фиксированный адрес: иначе правила фильтра пришлось бы переписывать
        # после каждой выдачи адреса
        if cfg.dut_mac:
            router.dhcp_reserve(cfg.dut_mac, cfg.dut_ip, 'waterius')
        router.client_stats(True)
        return stand

    def close(self) -> None:
        self.receiver.stop()
        self.router.close()

    def reset_observers(self) -> None:
        """Начать наблюдение с чистого листа - вызывается перед каждым тестом."""
        self.log.clear()
        self.receiver.drain()
        if self.mqtt:
            self.mqtt.drain()

    # --- наблюдение ---

    def wait_session(self, timeout: float = 180.0, mode: int | None = None) -> Session:
        """
        Дождаться завершённого сеанса и свести в него все три источника.

        Посылки берутся из очереди приёмника: она очищается перед тестом,
        значит всё, что там лежит, относится к этому сеансу. Последняя посылка -
        итоговая: если сервер прислал настройки, прошивка отправляет данные
        повторно, и актуальное состояние именно во второй.
        """
        session = self.log.wait_session(timeout, mode)
        assert session is not None, (
            f'сеанс не пришёл за {timeout:.0f} с '
            f'(ждали mode={mode})\n' + '\n'.join(self.log.lines[-40:]))

        while True:
            payload = self.receiver.wait_payload(timeout=1.0)
            if payload is None:
                break
            session.payloads.append(payload)
        if session.payloads:
            session.payload = session.payloads[-1]

        if self.mqtt:
            session.mqtt = [(m.topic, m.payload, m.retain) for m in self.mqtt.history]

        logger.info(f'сеанс: mode={session.mode}, посылок {len(session.payloads)}')
        return session

    def expect_no_session(self, timeout: float, mode: int | None = None) -> None:
        assert self.log.expect_no_session(timeout, mode), (
            f'ожидали тишину {timeout:.0f} с (mode={mode}), но сеанс состоялся\n'
            + '\n'.join(self.log.lines[-40:]))

    # --- настройка устройства ---

    def setup(self, channel: int | None = None, wake: bool = True,
              timeout: float = 180.0, **params: Any) -> Session:
        """
        Применить настройки и дождаться подтверждения.

        stand.setup(channel=1, factor=10, alarm_flow=3600)

        Настройки уезжают в теле ответа приёмника, прошивка их применяет и тут
        же отправляет данные повторно. Проверяем по двум признакам: строка
        `Apply setting:` в логе и новые значения во второй посылке. Без второй
        проверки тест поверил бы, что настройка применилась, хотя её отвергла
        валидация.
        """
        settings = self._translate(channel, params)
        logger.info(f'настройка: {settings}')

        self.reset_observers()
        self.receiver.reply_settings(settings)

        if wake:
            self.dut.press_button()

        session = self.wait_session(timeout=timeout)

        applied = session.applied
        missing = [k for k in settings if k not in applied]
        assert not missing, (
            f'прошивка не применила {missing}; в логе: {applied}\n{session.text}')

        assert len(session.payloads) >= 2, (
            'после применения настроек данные должны уйти повторно, '
            f'посылок {len(session.payloads)}')

        for name, value in settings.items():
            got = session.payload.get(name) if session.payload else None
            assert str(got) == str(value), (
                f'{name}: просили {value}, устройство отдаёт {got}')

        return session

    def setup_alarms(self, channel: int, timeout: float = 180.0, **params: Any) -> Session:
        """
        То же, но с обязательной проверкой, что пороги доехали в ОЗУ attiny.

        Без строки `Alarm config:` вся группа тревог бессмысленна: пороги живут
        в оперативной памяти attiny и уезжают туда отдельной командой в конце
        сеанса. Тест, не проверивший этого, зеленеет на выключенных тревогах.
        """
        session = self.setup(channel=channel, timeout=timeout, **params)

        config = session.alarm_config
        assert config is not None, (
            'в логе нет строки Alarm config - пороги не уехали в attiny '
            f'(версия attiny {session.attiny_version})\n{session.text}')

        if 'alarm_flow' in params and params['alarm_flow']:
            key = f'interval{channel}'
            assert config[key] > 0, f'{key} нулевой при заданном пороге: {config}'

        return session

    def _translate(self, channel: int | None, params: dict[str, Any]) -> dict[str, Any]:
        """Понятные имена -> параметры прошивки."""
        out: dict[str, Any] = {}
        for name, value in params.items():
            if name in GLOBAL_PARAMS:
                out[GLOBAL_PARAMS[name]] = value
            elif name in CHANNEL_PARAMS:
                assert channel is not None, f'{name} требует указания channel'
                out[f'{CHANNEL_PARAMS[name]}{channel}'] = value
            else:
                out[name] = value                 # имя параметра прошивки как есть
        return out

    # --- ожидание чистого состояния ---

    def wait_quiet(self, seconds: float = 300.0) -> None:
        """
        Дождаться, пока устройство перестанет будить себя по тревоге.

        Бюджет внеплановых сеансов обнуляется только плановым сеансом, поэтому
        остаток от предыдущего теста утёк бы в следующий и сломал счёт.
        """
        logger.info(f'ждём тишины {seconds:.0f} с')
        deadline = time.time() + seconds
        while time.time() < deadline:
            session = self.log.wait_session(timeout=30.0)
            if session is None:
                continue
            logger.info(f'в тишине случился сеанс mode={session.mode}, ждём дальше')
            deadline = time.time() + seconds
