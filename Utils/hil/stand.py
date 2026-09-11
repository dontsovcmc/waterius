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
from typing import TYPE_CHECKING, Any

from loguru import logger
from metf_python_client import METFClient

from .config import StandConfig
from .dut import Dut
from .logwatch import LogWatcher, Session
from .net import Net
from .receiver import Receiver
if TYPE_CHECKING:                     # paho нужен только тестам MQTT, а стенд
    from .mqttwatch import MqttWatch  # должен подниматься и без брокера
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

# Состояние, с которого начинается каждый тест. Без него результат зависит от
# того, что оставил предыдущий: test_I5 выключает квитанцию MQTT, а test_G1
# ничего не настраивает и требует, чтобы она была включена. Имена - как их
# печатает прошивка в посылке, чтобы сверять напрямую с ней, а не со своей
# памятью: настройки меняются и через MQTT, мимо setup().
BASELINE = {
    'vac': 0, 'sc': 0,
    'ackw': 1, 'ackh': 1, 'ackm': 1,
    'period_min': 120,
    'ctype0': 0, 'ctype1': 0,
    'cname0': 1, 'cname1': 0,      # красный вход - ГВС, синий - ХВС
    'f1': 10,
    'af0': 0, 'al0': 0, 'as0': 0,
    'af1': 0, 'al1': 0, 'as1': 0,
}

# Чем чинится залипшая тревога: настоящий порог, при котором ветка снятия в
# attiny достижима. Числа те же, что в тестах блока E.
NAMUR = 0
LEAKAGE_NC = 6          # нормально-замкнутый датчик, core/types.h
REPAIR_FACTOR = 10
REPAIR_FLOW = 3600      # л/ч
REPAIR_LEAK = 2         # минут непрерывного расхода

# Сколько ждём снятия перед очередным опросом кнопкой. Первый заход короткий:
# датчик протечки снимается за 750 мс, расход - за 20 с. Длинные паузы нужны
# только протечке по ритму, она ждёт двойного интервала между импульсами.
CLEAR_WAITS = (30.0, 120.0, 300.0)

GLOBAL_PARAMS = {
    'vacation': 'vac',
    'period_min': 'period_min',
    'send_on_consumption': 'sc',
    'confirm_waterius': 'ackw',
    'confirm_http': 'ackh',
    'confirm_mqtt': 'ackm',
}


def same_value(got: Any, want: Any) -> bool:
    """
    Одно ли это значение настройки.

    Флаги задаются числом (в прошивку они и уходят как "1"/"0"), а в посылке
    приезжают булевыми, поэтому сравнение строк дало бы вечное '1' != 'True'.
    Числа сравниваем как числа: показания задаются с литрами ("10.000"), а в
    посылке приезжают числом 10.0.
    """
    if isinstance(got, bool) or isinstance(want, bool):
        return bool(got) == bool(want)
    try:
        return abs(float(got) - float(want)) < 1e-6
    except (TypeError, ValueError):
        return str(got) == str(want)


class Stand:
    """Фасад над всем железом стенда."""

    def __init__(self, cfg: StandConfig, api: METFClient, router: NatRouter,
                 receiver: Receiver, mqtt: 'MqttWatch | None') -> None:
        self.cfg = cfg
        self.api = api
        self.router = router
        self.receiver = receiver
        self.mqtt = mqtt
        self.log = LogWatcher(api)
        self.dut = Dut(api, cfg.button_pin, cfg.ch0_pin, cfg.ch1_pin, cfg.reset_pin)
        self.net = Net(router, cfg.dut_ip, cfg.dut_mac,
                       cfg.broker_port, cfg.receiver_port)
        self.last_payload: dict[str, Any] | None = None
        self.attiny_version: int | None = None
        self.esp_version: tuple[int, int, int] | None = None
        self.dut_mac: str = cfg.dut_mac.lower()
        self.device_config: dict[str, str] = {}

    # --- жизненный цикл ---

    @classmethod
    def create(cls, cfg: StandConfig, mqtt: 'MqttWatch | None' = None) -> 'Stand':
        api = METFClient(cfg.metf_host)
        api.ping()
        api.serial_begin()
        router = connect(cfg.router_port or None, cfg.router_host or None,
                         cfg.router_password, cfg.ap_password)
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
        if session.payloads:
            session.payload = session.payloads[-1]
            self.last_payload = session.payload

        self._remember(session)

        if self.mqtt:
            session.mqtt = [(m.topic, m.payload, m.retain) for m in self.mqtt.history]

        logger.info(f'сеанс: mode={session.mode}, посылок {len(session.payloads)}')
        return session

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
        if session.mac and session.mac != self.dut_mac:
            if self.dut_mac:
                logger.warning(f'MAC из лога {session.mac} != {self.dut_mac} из stand.ini')
            self.dut_mac = session.mac

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
        session = self.wait_session(timeout=timeout)
        assert self.attiny_version is not None, (
            f'в логе нет версии attiny\n{session.text}')
        logger.info(f'устройство: attiny {self.attiny_version}, '
                    f'ЕСП {self.version_str}, MAC {self.dut_mac or "неизвестен"}')

        if self.dut_mac:
            self.router.dhcp_reserve(self.dut_mac, self.cfg.dut_ip)
            self.net.dut_mac = self.dut_mac

    def ensure_network(self, timeout: float = 300.0) -> bool:
        """
        Привести устройство в сеть стенда, если оно смотрит в чужую.

        Стенд бесполезен, пока Ватериус живёт в домашней сети: приёмник пуст,
        правила фильтра режут чужой адрес, а тесты валятся по причинам, к
        прошивке отношения не имеющим. Проверять это глазами - значит однажды
        прогнать весь набор впустую, поэтому проверка стоит до первого теста.

        Сверяемся с тем, что устройство напечатало о себе само, а имя сети
        спрашиваем у точки доступа: держать его третьей копией в stand.ini
        значит однажды переименовать точку и не заметить.

        Возвращает True, если пришлось настраивать.
        """
        want_ssid = self.ap_ssid
        want_url = self.cfg.http_url

        config = self.device_config
        assert config, ('устройство не напечатало свои настройки: '
                        'нечего сверять, проверьте лог и уровень логирования')
        if (config.get('wifi_ssid') == want_ssid
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

        session = self.wait_session(timeout=timeout)
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
        from .atboard import AtBoard
        from . import portal

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

    def ensure_mqtt(self, timeout: float = 300.0) -> bool:
        """
        Направить устройство в брокер стенда, если оно публикует не туда.

        Признак «настроено» - не строка в конфиге, а сообщение, пришедшее в наш
        брокер: адрес может совпадать, а публикации не быть (выключен MQTT,
        занят чужим брокером, не пускает сеть), и тогда весь блок I падал бы по
        причине, к прошивке отношения не имеющей.

        Возвращает True, если пришлось настраивать.
        """
        if self.mqtt is None:
            return False

        root = self.mqtt_root
        message = self.mqtt.wait_prefix(root, timeout=0)
        if message is not None:
            logger.info(f'устройство публикует в брокер стенда: {message.topic}')
            return False

        config = self.device_config
        logger.warning(
            f'устройство не публикует в брокер стенда: MQTT '
            f'{"ON" if config.get("mqtt_on") == "1" else "OFF"}, '
            f'{config.get("mqtt_host") or "?"}:{config.get("mqtt_port") or "?"}')

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

    def clear_alarms(self) -> None:
        """
        Снять тревогу, поднятую прошлым тестом.

        Три тревоги снимаются тремя разными способами, и способ выбирается по
        типу входа из последней посылки:

        - датчик протечки - отпустить вход (нормально-замкнутому, наоборот,
          замкнуть). Опрашивается вход, только пока его тип - датчик, поэтому
          сделать это надо до возврата типа в NAMUR: иначе снимать тревогу
          станет некому (`Attiny85/src/main.cpp`, alarm_tick);
        - большой расход - вернуть каналу настоящий порог. Ветка снятия в
          attiny начинается с проверки `min_interval`, и при нулевом пороге
          недостижима (`Attiny85/src/alarm.h`, on_tick);
        - протечка по ритму - просто тишина на линии, порога не спрашивает.

        Состояние читается кнопкой, а не ожиданием. Внеплановые сеансы у attiny
        по бюджету (ALARM_MAX_SESSIONS), и как только он исчерпан, устройство
        молчит до планового пробуждения - на стенде это два часа. Кнопка даёт
        сеанс сразу и заодно возвращает бюджет: при исчерпанном бюджете
        alarm_pending() ложно, и attiny засчитывает пробуждение как плановое.
        """
        payload = self.last_payload or {}
        names = ('alarm_flow', 'alarm_leak', 'alarm_wet')
        raised = {ch: [name for name in names if payload.get(f'{name}{ch}')]
                  for ch in (0, 1)}
        raised = {ch: found for ch, found in raised.items() if found}
        if not raised:
            return

        for channel, found in raised.items():
            logger.warning(f'канал {channel}: тревога прошлого теста ({found}), снимаем')
            self._start_clearing(channel, found, payload)

            for wait in CLEAR_WAITS:
                time.sleep(wait)
                self.reset_observers()
                self.dut.press_button()
                current = self.wait_session(timeout=180).payload or {}
                if not any(current.get(f'{name}{channel}') for name in found):
                    break
            else:
                raise AssertionError(
                    f'канал {channel}: тревога {found} не снялась')

            # У датчика протечки порога нет, обнулять нечего: пустой setup
            # не дал бы прошивке что применить, а она в ответ - второй посылки
            thresholds = {name: 0 for name in found if name != 'alarm_wet'}
            if thresholds:
                self.setup(channel=channel, **thresholds)

    def _start_clearing(self, channel: int, found: list[str],
                        payload: dict[str, Any]) -> None:
        """Привести канал в состояние, в котором attiny тревогу снимет."""
        if 'alarm_wet' in found:
            # У нормально-замкнутого датчика спокойное состояние - замкнутый
            # контакт, у обычного - разомкнутый.
            ctype = payload.get(f'ctype{channel}')
            self.dut.wet(channel=channel, closed=(ctype == LEAKAGE_NC))

        thresholds: dict[str, Any] = {}
        if 'alarm_flow' in found:
            thresholds['alarm_flow'] = REPAIR_FLOW
        if 'alarm_leak' in found:
            thresholds['alarm_leak'] = REPAIR_LEAK
        if thresholds:
            self.setup_alarms(channel=channel, ctype=NAMUR,
                              factor=REPAIR_FACTOR, vacation=0, **thresholds)

    def ensure_baseline(self) -> None:
        """
        Привести устройство к BASELINE перед тестом.

        Сверяемся с последней посылкой - это то, что устройство сообщает о себе
        само. Совпало всё - сеанса не будет: на живом железе он стоит полторы
        минуты, и платить их за каждый тест незачем.

        Поднятую тревогу снимаем до этого: BASELINE обнуляет пороги и
        возвращает входам тип NAMUR, а после этого снять тревогу нечем - ни
        порога для ветки снятия расхода, ни опроса входа для датчика.
        """
        self.clear_alarms()
        payload = self.last_payload
        if payload is None:
            diff = dict(BASELINE)
        else:
            diff = {name: value for name, value in BASELINE.items()
                    if name in payload and not same_value(payload[name], value)}
        if not diff:
            return
        logger.info(f'возврат к базовому состоянию: {diff}')
        self.setup(**diff)

    # --- ожидание чистого состояния ---

    def wait_quiet(self, seconds: float = 300.0, limit: float = 1800.0) -> None:
        """
        Дождаться, пока устройство перестанет будить себя по тревоге.

        Бюджет внеплановых сеансов обнуляется только плановым сеансом, поэтому
        остаток от предыдущего теста утёк бы в следующий и сломал счёт.

        `limit` - общий потолок ожидания. Без него незакрытая тревога держит
        фикстуру бесконечно: каждый сеанс продлевает срок, и по логу это
        неотличимо от зависшего стенда.
        """
        logger.info(f'ждём тишины {seconds:.0f} с')
        started = time.time()
        deadline = started + seconds
        sessions = 0
        while time.time() < deadline:
            if time.time() - started > limit:
                raise AssertionError(
                    f'устройство будит себя дольше {limit / 60:.0f} минут '
                    f'({sessions} сеансов): тревога прошлого теста не снята')
            session = self.log.wait_session(timeout=30.0)
            if session is None:
                continue
            sessions += 1
            logger.info(f'в тишине случился сеанс mode={session.mode}, ждём дальше')
            deadline = time.time() + seconds
