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
import warnings
from collections.abc import Mapping
from typing import TYPE_CHECKING, Any

from loguru import logger

from .clock import BoardClock
from .config import StandConfig
from .constants import (
    BASE_FACTOR,
    BUTTON_SESSION_WAIT_S,
    LEAKAGE_NC,
    NAMUR,
    WAKE_SETTLE_S,
    WATER_COLD,
    WATER_HOT,
)
from .dut import Dut
from .logwatch import MANUAL_TRANSMIT_MODE, WAKE_SESSION, LogWatcher, Session
from .metf import Metf
from .metf import check as metf_check
from .net import Net
from .receiver import Receiver
from .state import merge, same_value, unmet

if TYPE_CHECKING:                     # paho нужен только тестам MQTT, а стенд
    from .mqttwatch import MqttWatch  # должен подниматься и без брокера
from .router import NatRouter, connect

# Понятные имена вместо параметров прошивки. Хранятся здесь, а не в тестах,
# чтобы при переименовании параметра правка была одна.
CHANNEL_PARAMS = {
    'factor': 'f',
    'alarm_vol': 'av',
    'alarm_rate': 'ar',
    'alarm_hours': 'ah',
    'alarm_stop': 'as',
    'ctype': 'ctype',
    'serial': 'serial',
    'cname': 'cname',
    'value': 'ch',
}

# Настройки устройства, которые требуются от него в каждом тесте, если тест не
# объявил другие (маркер needs). Без них результат зависит от того, что оставил
# предыдущий: test_I5 выключает квитанцию MQTT, а test_G1 ничего не настраивает
# и требует, чтобы она была включена. Имена - параметров прошивки, как они
# уходят в ответе сервера. Сеть, брокер и часы добавляет Stand.requirements:
# их значения берутся из stand.ini.
BASELINE = {
    'vac': 0, 'sc': 0,
    'ackw': 1, 'ackh': 1, 'ackm': 1,
    'period_min': 120,
    'ctype0': NAMUR, 'ctype1': NAMUR,
    'cname0': WATER_HOT, 'cname1': WATER_COLD,  # красный вход - ГВС, синий - ХВС
    'f1': BASE_FACTOR,
    'av0': 0, 'ar0': 0, 'ah0': 0, 'as0': 0,
    'av1': 0, 'ar1': 0, 'ah1': 0, 'as1': 0,
}


# Сколько раз пробуем снять тревогу кнопкой. Одного нажатия достаточно, второе
# нужно на случай потерянного сеанса - лучше лишние двадцать секунд, чем
# упавший на чужой тревоге следующий тест.
CLEAR_TRIES = 3

# Сколько ждём стартового лога после нажатия. attiny подаёт питание сразу
# (main.cpp: esp.power за button.reset), баннер ЕСП идёт через 70 мс.
DEVICE_ALIVE_S = 2.0

GLOBAL_PARAMS = {
    'vacation': 'vac',
    'period_min': 'period_min',
    'send_on_consumption': 'sc',
    'confirm_waterius': 'ackw',
    'confirm_http': 'ackh',
    'confirm_mqtt': 'ackm',
}


# Порог прошивки: `ESP8266/src/voltage.h`, ALERT_POWER_DIFF_MV
ALERT_POWER_DIFF_MV = 100

# Сколько опрашиваем METF, прежде чем считать её пропавшей. На слабой связи
# плата переподключается сама, и одна осечка не значит ничего.
METF_WAIT_S = 30.0
METF_POLL_S = 3.0


def _wait_metf(api: Metf, host: str) -> float:
    """Дождаться METF. Вернуть, сколько ждали; 0 - отозвалась сразу."""
    started = time.monotonic()
    deadline = started + METF_WAIT_S
    last: Exception | None = None
    while True:
        try:
            api.ping()
        except Exception as err:
            last = err
            if time.monotonic() >= deadline:
                raise AssertionError(
                    f'стенд: METF {host} не отозвалась за {METF_WAIT_S:.0f} с: '
                    f'без неё нечем ни жать кнопку, ни читать лог. Последняя '
                    f'ошибка\n{last}') from last
            time.sleep(METF_POLL_S)
            continue
        return 0.0 if last is None else time.monotonic() - started


class StandPowerWarning(UserWarning):
    """Питание стенда просело настолько, что прошивка сочла батарейки севшими."""


class Stand:
    """Фасад над всем железом стенда."""

    def __init__(self, cfg: StandConfig, api: Metf, router: NatRouter,
                 receiver: Receiver, mqtt: MqttWatch | None) -> None:
        self.cfg = cfg
        self.api = api
        self.router = router
        self.receiver = receiver
        self.mqtt = mqtt
        self.log = LogWatcher(api)
        self._power_warned = False
        # Паузы между импульсами тоже вычитывают лог: иначе кольцо METF
        # переполняется плановым сеансом, и тест падает на неполном логе
        self.dut = Dut(api, cfg.button_pin, cfg.ch0_pin, cfg.ch1_pin, cfg.reset_pin,
                       idle=self.log.poll)
        # Время устройству отдаёт та же плата: тесты синхронизации не должны
        # зависеть ни от интернета, ни от серверов на машине с прогоном
        self.clock = BoardClock(cfg.metf_host)
        self.net = Net(router, cfg.dut_ip, cfg.dut_mac,
                       cfg.broker_port, cfg.receiver_port)
        self.last_payload: dict[str, Any] | None = None
        self.attiny_version: int | None = None
        self.esp_version: tuple[int, int, int] | None = None
        self.dut_mac: str = cfg.dut_mac.lower()
        self.device_config: dict[str, str] = {}
        # Последнее известное состояние устройства (state.py). None - неизвестно,
        # и следующий тест начнёт с короткого нажатия
        self.state: dict[str, Any] | None = None

    # --- жизненный цикл ---

    @classmethod
    def create(cls, cfg: StandConfig, mqtt: MqttWatch | None = None) -> Stand:
        api = Metf(cfg.metf_host)
        waited = _wait_metf(api, cfg.metf_host)
        try:
            api.serial_begin()
        except Exception as err:
            raise AssertionError(
                f'стенд: METF {cfg.metf_host} отвечает, но не открыла UART '
                f'устройства: читать лог нечем\n{err}') from err
        if waited:
            logger.info(f'стенд: METF {cfg.metf_host} отозвалась через {waited:.0f} с')

        router_at = cfg.router_port or cfg.router_host
        try:
            router = connect(cfg.router_port or None, cfg.router_host or None,
                             cfg.router_password, cfg.ap_password)
            version = router.version()
        except Exception as err:
            raise AssertionError(
                f'стенд: WT32-ETH01 {router_at} - нет: консоль не отвечает\n{err}') from err
        logger.info(f'стенд: WT32-ETH01 {router_at} - есть, {version}')

        # Досмотр METF идёт после роутера: имя точки стенда спрашивается у него,
        # а без имени не проверить главного - что управляющий канал не лежит на
        # том, что тесты ломают
        ap = router.config()
        metf_check(api, cfg.metf_host, cfg.ap_ssid or ap.get('ssid', ''),
                   int(ap.get('channel', 0) or 0))

        # Прошлый прогон мог умереть с выключенной точкой или правилом фильтра.
        # restore() чинит это после теста, а identify() идёт раньше первого
        router.acl_clear()
        router.ap(True)

        receiver = Receiver(port=cfg.receiver_port,
                            cert_host=cfg.receiver_host)
        receiver.start()

        stand = cls(cfg, api, router, receiver, mqtt)
        stand.dut.init()

        # Адрес закрепляется в identify(), когда MAC уже прочитан из лога:
        # правила фильтра иначе пришлось бы переписывать после каждой выдачи
        router.client_stats(True)
        return stand

    def close(self) -> None:
        self.receiver.stop()
        self.router.close()

    @property
    def ap_ssid(self) -> str:
        """
        Имя точки доступа стенда.

        Спрашиваем у самой точки, если не задано в stand.ini: третья копия
        имени однажды разойдётся с эфиром, и заметить это будет нечем.
        """
        ssid = self.cfg.ap_ssid or self.router.config().get('ssid', '')
        assert ssid, 'не удалось узнать имя точки доступа стенда'
        return ssid

    def reset_observers(self) -> None:
        """Начать наблюдение с чистого листа - вызывается перед каждым тестом."""
        self.log.clear()
        self.receiver.drain()
        if self.mqtt:
            self.mqtt.clear_retained_tree(self.mqtt_root)
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

        broken = self.receiver.take_broken()
        # Оборванное тело валит тест, только если целого в этом сеансе так и не
        # пришло. Прошивка отправляет трижды (`https_helpers.cpp`, Attempt #N),
        # и повтор после обрыва - её правильное поведение, а не дефект: обрыв
        # рождается в эфире стенда (05_air-and-loss.md), а не в устройстве.
        # Требовать «ни одного обрыва» значило бы держать прогон заложником
        # своей же сети; молчать о нём тоже нельзя - отсюда предупреждение.
        #
        # К отказу прикладываем взгляд самого устройства: сколько оно собиралось
        # отправить и что получило в ответ. Без этих строк отказ говорит только
        # «тело неполное», и виноватого ищут наугад - сеть, приёмник, прошивка.
        if broken:
            sending = '\n'.join(line for line in session.text.splitlines()
                                 if 'HTTP' in line or 'WATR' in line)
            assert session.payloads, (
                f'приёмник получил {len(broken)} нечитаемое тело посылки и ни '
                f'одного целого: передача оборвалась на середине, и о состоянии '
                f'устройства этот сеанс не говорит ничего. '
                f'Начало первого: {broken[0][:200]!r}\n'
                f'Что об этой отправке говорит устройство:\n{sending}')
            logger.warning(
                f'обрыв тела посылки ({len(broken)} шт.), но повтор дошёл целым: '
                f'эфир стенда теряет пакет, устройство отработало верно')

        if session.payloads:
            session.payload = session.payloads[-1]
            self.last_payload = session.payload

        self._remember(session)

        if self.mqtt:
            session.mqtt = [(m.topic, m.payload, m.retain) for m in self.mqtt.history]

        if session.payload is not None:
            self._check_power(session.payload)

        logger.info(f'сеанс: mode={session.mode}, посылок {len(session.payloads)}')
        return session

    def _check_power(self, payload: Mapping[str, Any]) -> None:
        """
        Просадка питания - беда стенда, а не прошивки.

        Прошивка считает батарейки севшими, если замеры за сеанс разошлись на
        100 мВ (`ESP8266/src/voltage.h`, ALERT_POWER_DIFF_MV), и моргает кодом
        1 - у питаемого от стенда устройства это говорит о кабеле и источнике.
        Предупреждение одно на прогон: оно про стенд, а не про тест.
        """
        if not payload.get('voltage_low') or self._power_warned:
            return
        self._power_warned = True
        diff_mv = round(float(payload.get('voltage_diff') or 0.0) * 1000)
        text = (f'стенд: питание устройства просело на {diff_mv} мВ за сеанс при '
                f'пороге прошивки {ALERT_POWER_DIFF_MV} мВ '
                f'(напряжение {payload.get("voltage")} В). Прошивка считает это '
                f'разряженными батарейками и моргает кодом 1. Лечится питанием: '
                f'короче кабель, отдельный источник, ёмкость по питанию платы')
        logger.warning(text)
        warnings.warn(text, StandPowerWarning, stacklevel=2)

    def wait_asleep(self, timeout: float = 60.0) -> bool:
        """
        Дождаться, пока устройство уснёт: строки `Going to sleep`.

        Одного конца сеанса мало. Из режима настройки прошивка уходит
        перезапуском, и такой сеанс кончается не сном, а следующим включением -
        ЕСП после него доигрывает обычный сеанс и только тогда засыпает. Пока
        она запитана, нажатие кнопки до attiny не доходит, и тест, начавшийся в
        это время, нажал бы впустую и ждал бы своего сеанса до таймаута.

        Перезапуск с передачей укладывается в десяток секунд; минуты хватает с
        запасом, а сеанс, не кончившийся за неё, не кончится вовсе - через две
        минуты attiny снимет питание молча (`WAIT_ESP_MSEC`).
        """
        while True:
            session = self.log.wait_session(timeout)
            if session is None:
                logger.warning(f'устройство не уснуло за {timeout:.0f} с')
                return False
            logger.info(f'доиграл сеанс mode={session.mode}')
            if session.complete:
                return True

    def expect_no_session(self, timeout: float, mode: int | None = None) -> None:
        assert self.log.expect_no_session(timeout, mode), (
            f'ожидали тишину {timeout:.0f} с (mode={mode}), но сеанс состоялся\n'
            + '\n'.join(self.log.lines[-40:]))

    def _remember(self, session: Session) -> None:
        """Запомнить то, что устройство рассказало о себе в этом сеансе."""
        if session.attiny_version is not None:
            self.attiny_version = session.attiny_version
        if session.esp_version is not None:
            self.esp_version = session.esp_version
        config = session.config
        if config:
            self.device_config = config
        if config or session.payload is not None or session.saved:
            self.state = merge(self.state, config, session.payload, session.saved)
        if session.mac and session.mac != self.dut_mac:
            if self.dut_mac:
                logger.warning(f'MAC из лога {session.mac} != {self.dut_mac} из stand.ini')
            self.dut_mac = session.mac

    def check_atboard(self, timeout: float = 10.0) -> None:
        """
        AT-плата на месте. Нужна только тестам портала, но узнать, что её нет,
        лучше до первого теста, чем на сороковой минуте прогона.
        """
        port = self.cfg.atboard_port
        if not port:
            logger.warning('стенд: AT-плата не задана ([atboard] port) - '
                           'тесты портала будут пропущены')
            return

        from .atboard import AtBoard, AtError
        try:
            board = AtBoard(port)
        except Exception as err:
            raise AssertionError(f'стенд: AT-плата {port} - нет: порт не открылся\n{err}') from err
        try:
            # Открытие порта перезагружает NodeMCU: первые команды тонут (atboard.py, wait_ready)
            deadline = time.time() + timeout
            answer = ''
            while 'OK' not in answer and time.time() < deadline:
                try:
                    answer = board.cmd('AT', timeout=1)
                except AtError:
                    pass
            assert 'OK' in answer, (
                f'стенд: AT-плата {port} - нет: на `AT` нет OK за {timeout:.0f} с, '
                f'ответ {answer!r}')
        finally:
            board.close()
        logger.info(f'стенд: AT-плата {port} - есть')

    def expect_awake(self, timeout: float = DEVICE_ALIVE_S) -> None:
        """
        Убедиться, что Ватериус проснулся, - сразу после нажатия кнопки.

        Отсутствие устройства и спящее устройство по логу неотличимы: между
        сеансами оно молчит всегда. Отличает их нажатие, после которого ЕСП
        печатает стартовый лог. Нажимает вызывающий - своего нажатия здесь нет
        намеренно, второе подряд ЕСП проглотит, она уже не спит.

        Разбор стартового лога - `LogWatcher.wait_wake`: он отличает молчание,
        живую ЕСП без attiny и лог без начала сеанса (программатор на том же
        UART рвёт поток), и каждое называет своими словами.
        """
        try:
            self.dut.api.ping()
        except Exception as err:
            raise AssertionError(
                f'плата METF {self.cfg.metf_host} не отвечает: без неё стенду '
                f'нечем ни жать кнопку, ни читать лог\n{err}') from err

        wake = self.log.wait_wake(timeout)
        logger.info(f'стартовый лог за {wake.waited:.1f} с: {wake.dump()}')
        if wake.state != WAKE_SESSION:
            raise AssertionError(
                wake.describe(f'кнопки METF (GPIO{self.cfg.button_pin})'))

    def identify(self, timeout: float = 180.0) -> None:
        """
        Один сеанс в начале прогона: узнать версии и MAC у самого устройства.

        Версии нужны до первого теста - по ним решается, какие тесты вообще
        имеют смысл на этой прошивке. MAC оттуда же: держать его в stand.ini
        значит однажды прогнать тесты против чужого адреса и разбираться, почему
        правила фильтра ничего не режут.
        """
        self.reset_observers()
        self.dut.press_button()
        self.expect_awake()
        session = self.wait_session(timeout=timeout)
        assert self.attiny_version is not None, (
            f'в логе нет версии attiny\n{session.text}')
        logger.info(f'стенд: Ватериус - есть: attiny {self.attiny_version}, '
                    f'ЕСП {self.version_str}, MAC {self.dut_mac or "неизвестен"}')

        if self.dut_mac:
            self.router.dhcp_reserve(self.dut_mac, self.cfg.dut_ip)
            self.net.dut_mac = self.dut_mac

    def ensure_network(self, timeout: float = 300.0, force: bool = False) -> bool:
        """
        Привести устройство в сеть стенда, если оно смотрит в чужую.

        Стенд бесполезен, пока Ватериус живёт в домашней сети: приёмник пуст,
        правила фильтра режут чужой адрес, а тесты валятся по причинам, к
        прошивке отношения не имеющим. Проверять это глазами - значит однажды
        прогнать весь набор впустую, поэтому проверка стоит до первого теста.

        Сверяемся с тем, что устройство напечатало о себе само, а имя сети
        спрашиваем у точки доступа: держать его третьей копией в stand.ini
        значит однажды переименовать точку и не заметить.

        force=True настраивает заново и при совпавших настройках: так возвращают
        в сеть устройство, у которого испорчен пароль, а имя сети и сервер
        прежние.

        Возвращает True, если пришлось настраивать.
        """
        want_ssid = self.ap_ssid
        want_url = self.cfg.http_url

        config = self.device_config
        assert config, ('устройство не напечатало свои настройки: '
                        'нечего сверять, проверьте лог и уровень логирования')
        if (not force and config.get('wifi_ssid') == want_ssid
                and config.get('http_on') == '1'
                and config.get('http_host') == want_url):
            logger.info(f'устройство уже в сети стенда: {want_ssid} -> {want_url}')
            return False

        logger.warning(f'устройство настроено на чужую сеть: '
                       f'{config.get("wifi_ssid") or "?"} -> '
                       f'{config.get("http_host") or "нет своего сервера"}')
        assert self.cfg.atboard_port, (
            'нужна AT-плата, чтобы настроить устройство через портал: '
            '[atboard] port в stand.ini. Либо настройте Ватериус вручную на сеть '
            f'{want_ssid} и сервер {want_url}')
        assert self.cfg.ap_password, (
            '[router] ap_password в stand.ini: пароль точки доступа стенда, '
            'его не прочитать у роутера - show config печатает звёздочки')

        self._setup_via_portal(want_ssid, want_url)

        # Первым в логе лежит сеанс самого портала, и настройки он напечатал до
        # сохранения. Сеанс после «Завершить» attiny помечает MANUAL_TRANSMIT
        # (SlaveI2C.cpp, команда 'T')
        session = self.wait_session(timeout=timeout, mode=MANUAL_TRANSMIT_MODE)
        config = session.config
        assert config.get('wifi_ssid') == want_ssid, (
            f'после настройки сеть осталась {config.get("wifi_ssid")!r}\n{session.text}')
        assert config.get('http_host') == want_url, (
            f'после настройки сервер остался {config.get("http_host")!r}\n{session.text}')
        assert session.wifi_connected, (
            f'устройство не подключилось к {want_ssid}\n{session.text}')
        assert session.payload is not None, (
            'устройство в сети стенда, но посылка до приёмника не дошла. '
            f'NAT на точке: {self.router.nat_enabled()}. Клиенты точки видят '
            'только её саму, если NAT не поднялся - лечится `restart` роутера; '
            f'приёмник слушает {self.cfg.http_url}\n{session.text}')
        logger.info(f'устройство переведено в сеть стенда: {want_ssid} -> {want_url}')
        return True

    def _setup_via_portal(self, ssid: str, url: str) -> None:
        """Режим настройки, форма портала через AT-плату, выход из режима."""
        from . import portal
        from .atboard import AtBoard

        self.log.clear()
        self.dut.hold_button()
        ap = None
        deadline = time.time() + 90
        while time.time() < deadline and not ap:
            self.log.poll()
            ap = portal.find_ap(self.log.lines)
            if not ap:
                time.sleep(1)
        assert ap, 'точка доступа портала не поднялась после длинного нажатия'

        board = AtBoard(self.cfg.atboard_port)
        try:
            logger.info(f'AT-плата в сети портала {ap}: {board.join(ap)}')
            portal.configure(board, ssid, self.cfg.ap_password, url)
        finally:
            board.close()

    @property
    def mqtt_root(self) -> str:
        """
        Корневой топик устройства.

        Его задаёт стенд, а не устройство: в лог прошивка топик не печатает
        (`config.cpp`, print_settings), значит прочитать его неоткуда, а
        угадывать - значит однажды слушать пустое дерево и списать это на
        сломанный MQTT.
        """
        return self.cfg.mqtt_topic.rstrip('/')

    def ensure_clock(self, timeout: float = 300.0) -> None:
        """
        Дать устройству источник времени на стенде, а не в интернете.

        Иначе время приходит из настоящего пула, и стенд молча зависит от
        интернета: пропал - и подстройка периода замирает, счётчик неудач
        растёт, метка в посылке идёт по оценке. Ни один тест от этого не
        покраснеет, просто проверять будет слегка другое устройство.

        Часы платы ставятся настоящие: сдвиг - приём блока N и только его,
        во всех остальных метка времени сверяется с реальной.

        Адрес сервера в посылку не попадает (`json.cpp` отдаёт только
        `ntp_errors`), поэтому в эталон его не положить и сверять при каждом
        тесте нечем: ставим один раз за прогон, а блок N возвращает своё сам.
        """
        assert self.clock.available(), (
            f'на плате {self.cfg.metf_host} нет сервера времени: нужен METF 6. '
            f'Прошейте плату - иначе время придёт из интернета, и прогон будет '
            f'зависеть от него молча')

        self.clock.start(int(time.time()))
        self.setup(ntp_server=self.cfg.metf_host, timeout=timeout)
        logger.info(f'источник времени устройства: плата {self.cfg.metf_host}')

    def arm_clock(self) -> None:
        """
        Переподнять часы платы, если она их потеряла.

        Часов реального времени у платы нет: перезагрузка или перепрошивка
        стирают назначенный момент, и сервер начинает молча выбрасывать
        запросы. Проверка стоит одного HTTP-запроса и ни одного сеанса
        устройства, поэтому её не жалко делать перед каждым тестом.
        """
        if self.clock.stat()['running']:
            return

        logger.warning('плата потеряла часы, поднимаем заново')
        self.clock.start(int(time.time()))

    def ensure_mqtt(self, timeout: float = 300.0) -> bool:
        """
        Направить устройство в брокер стенда, если оно публикует не туда.

        Признак «настроено» - не строка в конфиге, а сообщение, пришедшее в наш
        брокер: адрес может совпадать, а публикации не быть (выключен MQTT,
        занят чужим брокером, не пускает сеть), и тогда весь блок I падал бы по
        причине, к прошивке отношения не имеющей.

        Топик задаём всегда, а не только когда в брокере тихо. По началу топика
        устройство на заводских настройках неотличимо от настроенного: его
        топик по умолчанию - `<корень>/<номер чипа>/` (`config.cpp`, начальные
        настройки), то есть тот же префикс. Стенд считал, что всё в порядке, а
        тест ждал показаний в корне и падал на пустом месте: в брокере лежало
        `waterius/6827706`. Прочитать топик неоткуда - в лог прошивка его не
        печатает, - значит единственный честный путь - назначить свой и
        убедиться строкой `Saved:`, что он принят (это делает setup).

        Возвращает True, если брокер есть и устройство в него направлено.
        """
        if self.mqtt is None:
            return False

        root = self.mqtt_root

        config = self.device_config
        logger.info(
            f'переводим устройство в брокер стенда {self.cfg.broker_host}:'
            f'{self.cfg.broker_port}, топик «{root}» (сейчас MQTT '
            f'{"ON" if config.get("mqtt_on") == "1" else "OFF"}, '
            f'{config.get("mqtt_host") or "?"}:{config.get("mqtt_port") or "?"})')

        # mqtt_on обязан идти первым: адрес, порт и топик прошивка принимает
        # только при включённом MQTT (active_point_api.cpp,
        # applyNonCheckBoxParameter), а параметры применяются в порядке ключей
        # ответа.
        self.setup(mqtt_on=1,
                   mqtt_host=self.cfg.broker_host,
                   mqtt_port=self.cfg.broker_port,
                   mqtt_topic=root,
                   timeout=timeout)

        message = self.mqtt.wait_prefix(root, timeout=30)
        if message is None:
            # На прошивках младше 2.0.47 повторная посылка того же сеанса до
            # брокера не доезжает, поэтому проверяем следующим сеансом, где
            # MQTT включён с самого начала.
            self.reset_observers()
            self.dut.press_button()
            session = self.wait_session(timeout=timeout)
            message = self.mqtt.wait_prefix(root, timeout=30)
            assert message is not None, (
                f'устройство не опубликовало ничего в {root}/ на '
                f'{self.cfg.broker_host}:{self.cfg.broker_port}. '
                f'Подключение к брокеру: '
                f'{"есть" if "MQTT: Connected." in session.text else "нет"}\n'
                f'{session.text}')

        logger.info(f'устройство переведено в брокер стенда: {message.topic}')
        return True

    @property
    def version_str(self) -> str:
        return '.'.join(str(x) for x in self.esp_version) if self.esp_version else '?'

    # --- настройка устройства ---

    def setup(self, channel: int | None = None, wake: bool = True,
              timeout: float = 180.0, **params: Any) -> Session:
        """
        Применить настройки и дождаться подтверждения.

        stand.setup(channel=1, factor=10, alarm_vol=50)

        Настройки уезжают в теле ответа приёмника, прошивка их применяет и тут
        же отправляет данные повторно. Проверяем по двум признакам: строка
        `Apply setting:` в логе и новые значения во второй посылке. Без второй
        проверки тест поверил бы, что настройка применилась, хотя её отвергла
        валидация.

        Сеанс заказывается кнопкой, а она снимает все тревоги attiny
        (`Attiny85/src/main.cpp`, ButtonPressType::SHORT). Поэтому тест,
        проверяющий снятие чем-то другим - маской, сменой типа входа,
        выключением отпуска, - обязан звать setup с wake=False и ждать
        планового сеанса: иначе он проверит собственное нажатие и будет зелёным
        при любой прошивке.
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

        payload = session.payload or {}
        saved = session.saved
        for name, value in settings.items():
            if name in payload:
                assert same_value(payload[name], value), (
                    f'{name}: просили {value}, устройство отдаёт {payload[name]}')
                continue
            # Адреса, порты и включённость получателей в посылку не попадают
            # (json.cpp), поэтому для них единственное свидетельство - строка
            # `Saved:`: её печатают после валидации, а отвергнутый параметр
            # уходит в ошибку и такой строки не оставляет.
            assert name in saved, (
                f'{name} не виден ни в посылке, ни строкой Saved: '
                f'прошивка его не приняла\n{session.text}')
            assert saved[name] == str(value), (
                f'{name}: просили {value}, прошивка сохранила {saved[name]}')

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

        # Порог, заданный в человеческих единицах, обязан доехать числом для
        # attiny. Проверка не формальная: пересчёт делится на вес импульса, и
        # при незаданном весе даёт ноль - то есть выключенную тревогу.
        checks = (('alarm_vol', f'vol{channel}'),
                  ('alarm_rate', f'quantum{channel}'),
                  ('alarm_hours', f'quanta{channel}'))
        for name, key in checks:
            if params.get(name):
                assert config[key] > 0, (
                    f'{key} нулевой при заданном {name}={params[name]}: {config}')

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

    def clear_alarms(self) -> None:
        """
        Снять тревогу, поднятую прошлым тестом.

        Ни одна тревога attiny не гаснет сама - ни по времени, ни по
        прекращению расхода (`Attiny85/src/alarm.h`). Поэтому способ снятия
        ровно один и общий для всех трёх: короткое нажатие кнопки, которое
        снимает обе маски разом (`Attiny85/src/main.cpp`, ButtonPressType::SHORT).

        Исключение - датчик протечки: `set_wet` поднимает тревогу заново на
        ближайшем же тике, пока вход замкнут. Значит вход надо отпустить до
        нажатия, и сделать это, пока тип входа ещё датчик: опрашивается он
        только в этом типе (`alarm_tick`).

        Состояние читается кнопкой, а не ожиданием: сеанс по ней приходит сразу
        и привозит посылку, по которой видно, снялось ли.
        """
        payload = self.last_payload or {}
        names = ('alarm_flow', 'alarm_leak', 'alarm_wet')
        raised = {ch: [name for name in names if payload.get(f'{name}{ch}')]
                  for ch in (0, 1)}
        raised = {ch: found for ch, found in raised.items() if found}
        if not raised:
            return

        logger.warning(f'тревоги прошлого теста: {raised}, снимаем кнопкой')

        for channel, found in raised.items():
            if 'alarm_wet' in found:
                # У нормально-замкнутого датчика спокойное состояние - замкнутый
                # контакт, у обычного - разомкнутый.
                ctype = payload.get(f'ctype{channel}')
                self.dut.wet(channel=channel, closed=(ctype == LEAKAGE_NC))

        left: list[str] = []
        for _ in range(CLEAR_TRIES):
            self.reset_observers()
            self.dut.press_button()
            current = self.wait_session(timeout=180).payload or {}
            left = [f'{name}{ch}' for ch, found in raised.items()
                    for name in found if current.get(f'{name}{ch}')]
            if not left:
                return

        raise AssertionError(f'тревоги {left} не снялись кнопкой')

    def read_state(self) -> None:
        """
        Узнать состояние коротким нажатием: сеанс привозит и посылку, и конфиг.

        Зовётся там, где неизвестно, чем кончил предыдущий тест, - в том числе
        сразу после портального блока, где плата только что доиграла сеанс.
        Отсюда выдержка перед нажатием: нажатие вплотную к снятию питания до
        attiny не доходит.
        """
        logger.info('состояние устройства неизвестно, читаем коротким нажатием')
        time.sleep(WAKE_SETTLE_S)
        self.reset_observers()
        self.dut.press_button()
        self.wait_session(timeout=BUTTON_SESSION_WAIT_S)

    def forget_state(self) -> None:
        """
        Состояние больше не известно: настройки менялись мимо стенда (портал,
        заводской сброс) или тест упал на середине, и чем он кончил, не видно.
        """
        self.state = None

    def requirements(self, extra: Mapping[str, Any] | None = None) -> dict[str, Any]:
        """
        Требования к устройству: общие для всех тестов плюс свои из маркера needs.

        Общие - то, без чего тест проверял бы не прошивку, а наследство соседа:
        сервер стенда, часы платы, брокер и BASELINE. Получатель стоит раньше
        адреса: адрес прошивка принимает только при включённом получателе, а
        параметры применяются в порядке ключей ответа.

        Автодискавери по умолчанию выключено: сеанс с ним печатает сотни строк и
        вытесняет из кольца METF начало следующего.
        """
        want: dict[str, Any] = {'http_on': 1, 'http_url': self.cfg.http_url,
                                'ntp_server': self.cfg.metf_host}
        if self.mqtt is not None:
            want.update(mqtt_on=1, mqtt_host=self.cfg.broker_host,
                        mqtt_port=self.cfg.broker_port, mqtt_retain=1,
                        mqtt_auto_discovery=0)
        want.update(BASELINE)
        want.update(extra or {})
        return want

    def ensure_requirements(self, extra: Mapping[str, Any] | None = None) -> None:
        """
        Привести устройство к требованиям теста, если оно им не отвечает.

        Неизвестное состояние сперва читается коротким нажатием. Совпало всё -
        сеанса с настройкой не будет: на живом железе он стоит полторы минуты.

        Поднятую тревогу снимаем до настройки: требования возвращают входам тип
        NAMUR, а датчик протечки опрашивается только в своём типе - отпустить
        вход после смены типа уже некому, и тревога вернулась бы.
        """
        if self.state is None:
            self.read_state()
        self.clear_alarms()
        diff = unmet(self.state or {}, self.requirements(extra))
        if not diff:
            return
        logger.info(f'настройка под требования теста: {diff}')
        self.setup(**diff)

    # --- ожидание чистого состояния ---

    def assert_no_alarms(self) -> Session:
        """
        Доказать, что тест начинается без поднятой тревоги.

        Бюджета внеплановых сеансов больше нет: каждая тревога поднимается один
        раз и молчит до снятия, поэтому ограничивать их число незачем
        (`Attiny85/src/alarm.h`). Осталось второе: чужая тревога, доставшаяся от
        предыдущего теста, ломает этот - он ждёт своего внепланового сеанса, а
        устройство про новость уже доложило.

        Проверяем посылкой, а не отсутствием сеансов: сеанс по кнопке стоит
        двадцати секунд, а «сеансов не было» доказывается только временем.
        """
        self.reset_observers()
        self.dut.press_button()
        session = self.wait_session(timeout=180)

        payload = session.payload or {}
        raised = [f'{name}{ch}' for ch in (0, 1)
                  for name in ('alarm_flow', 'alarm_leak', 'alarm_wet')
                  if payload.get(f'{name}{ch}')]
        assert not raised, (
            f'тест начинается с поднятой тревогой {raised}: снять её было некому, '
            f'сама она не гаснет\n{session.text}')
        return session
