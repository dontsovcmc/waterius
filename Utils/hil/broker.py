"""
MQTT-брокер на время прогона.

Стенд не должен зависеть от чужого брокера: тест, проверяющий retain или
автодискавери, обязан начинаться с чистых топиков. Поднимаем брокер в самом
процессе тестов и гасим в конце.

Брокер питоновский (amqtt), а не mosquitto: внешний бинарник ставится руками,
на каждой машине по-своему, и его отсутствие превращает весь блок MQTT в
молчаливый пропуск. Здесь он приезжает вместе с остальными зависимостями из
requirements.txt.

Хранилище выключено намеренно. Проверка «удерживаемое сообщение переживает
перезапуск брокера» - это проверка брокера, а не прошивки; вместо неё тест
подписывается заново и смотрит флаг retain в пришедшем сообщении.
"""

from __future__ import annotations

import asyncio
import logging
import threading

from loguru import logger


class MqttBroker:
    """Брокер внутри процесса pytest, на своём потоке с отдельным циклом."""

    def __init__(self, port: int = 1883, host: str = '') -> None:
        self.port = port
        # Адрес, по которому брокер ищет Ватериус: он приходит через NAT точки
        # доступа, значит слушать только localhost недостаточно. Готовность
        # проверяем по нему же, а не по 127.0.0.1.
        self.host = host or '127.0.0.1'
        self._broker: object | None = None
        self._loop: asyncio.AbstractEventLoop | None = None
        self._thread: threading.Thread | None = None

    @staticmethod
    def available() -> bool:
        try:
            import amqtt.broker           # noqa: F401
        except ImportError:
            return False
        return True

    def listening(self, timeout: float = 1.0) -> bool:
        """
        Отвечает ли на этом адресе брокер.

        Проверяем настоящим подключением MQTT, а не голым сокетом: оборванный
        на середине сокет брокер пишет в лог ошибкой, и потом эти строки ищут
        глазами среди настоящих.
        """
        import paho.mqtt.client as mqtt

        probe = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                            client_id='hil-probe')
        try:
            probe.connect(self.host, self.port, keepalive=5)
            probe.loop(timeout)
            probe.disconnect()
        except OSError:
            return False
        return True

    @staticmethod
    def _broker_class() -> type:
        """
        amqtt с заплатой на рассылку удерживаемых сообщений.

        Два дефекта в одном месте (`amqtt/broker.py`,
        `_publish_retained_messages_for_subscription` и цикл
        `for topic in self._subscriptions` в обработчике подключения), и оба
        меняют поведение устройства, а не только отчёт.

        Первый: при подключении клиента брокер рассылает ему удерживаемые
        сообщения по всем фильтрам, какие зарегистрировал хоть кто-нибудь, а не
        по его собственным. Ватериус на каждое полученное сообщение публикует
        пустое с флагом retain (`ha/subscribe.cpp`, clear_retained), то есть
        из-за подписки стенда на `#` он вычистил бы retain у собственных
        показаний и у автодискавери.

        Второй: сообщение, опубликованное с QoS 0, рассылается с QoS подписки -
        в `min(qos, retained.qos or qos)` ноль считается «не задано». По
        стандарту доставка идёт с наименьшим из двух (MQTT 3.1.1, 3.8.4), то
        есть нулём, и подтверждения не требует. С QoS 1 требует: Ватериус ждёт
        пакеты в буфер 256 байт (`senders/sender_mqtt.h`), свои же показания в
        девять сотен не принимает и PUBACK не шлёт - брокер ждёт его пять
        секунд и рвёт сеанс, а публикации этого пробуждения пропадают.

        Заплата рассылает только тому, кто на фильтр подписан, и с тем QoS,
        с каким сообщение опубликовали. Штатный путь (SUBSCRIBE) не страдает:
        подписка заносится в список до рассылки.
        """
        from amqtt.broker import Broker

        # Только методы: `_retained_messages` и `_subscriptions` заводятся в
        # конструкторе, у класса их нет, а пропажу видно первым же вызовом.
        needed = ('_publish_retained_messages_for_subscription',
                  '_get_handler', '_matches')
        missing = [name for name in needed if not hasattr(Broker, name)]
        if missing:
            raise RuntimeError(
                f'amqtt изменил внутренности ({missing}): заплата про удерживаемые '
                'сообщения больше не накладывается, проверьте broker.py')

        class StandBroker(Broker):                       # type: ignore[misc, valid-type]

            async def _publish_retained_messages_for_subscription(
                    self, subscription: tuple, session: object) -> None:
                topic_filter, qos = subscription
                holders = self._subscriptions.get(topic_filter, [])
                if not any(held is session for held, _ in holders):
                    return

                handler = self._get_handler(session)
                if handler is None:
                    return

                for topic, retained in self._retained_messages.items():
                    if not self._matches(topic, topic_filter):
                        continue
                    await handler.mqtt_publish(
                        retained.topic, retained.data,
                        min(qos, retained.qos or 0), retain=True)

        return StandBroker

    def start(self, timeout: float = 10.0) -> None:
        if not self.available():
            raise RuntimeError('нет amqtt: pip install -r Utils/hil/requirements.txt')

        # Чужой брокер на том же порту тесты бы не заметили: они читали бы его
        # топики, а retain и автодискавери пришли бы из прошлой жизни.
        if self.listening():
            raise RuntimeError(
                f'порт {self.port} уже занят - тесты читали бы чужой брокер')

        broker_class = self._broker_class()

        # Плагины перечислены явно: по умолчанию amqtt включает ещё два логгера
        # (событий и пакетов) и дерево $SYS - в отчёте о прогоне это шум.
        config = {
            'listeners': {'default': {'type': 'tcp',
                                      'bind': f'0.0.0.0:{self.port}'}},
            'plugins': {
                'amqtt.plugins.authentication.AnonymousAuthPlugin': {
                    # Логин и пароль у Ватериуса могли остаться от домашнего
                    # брокера: сбрасывать их ради стенда - лишний сеанс и лишняя
                    # настройка, которую потом возвращать.
                    'allow_anonymous': True},
            },
        }
        # amqtt и его конечный автомат (transitions) сыплют в лог каждым
        # переходом состояния клиента: в отчёте о прогоне это прячет
        # настоящие строки стенда.
        for name in ('amqtt', 'transitions'):
            logging.getLogger(name).setLevel(logging.WARNING)

        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(target=self._loop.run_forever,
                                        name='mqtt-broker', daemon=True)
        self._thread.start()

        async def boot() -> object:
            # Именно здесь, а не в главном потоке: часть внутренних объектов
            # amqtt привязывается к текущему циклу прямо в конструкторе, и
            # брокер, созданный снаружи, потом не глохнет - сигнал остановки
            # уходит в чужой цикл (broker.py: _broadcast_shutdown_waiter).
            broker = broker_class(config)
            await broker.start()
            return broker

        self._broker = asyncio.run_coroutine_threadsafe(
            boot(), self._loop).result(timeout)

        if not self.listening():
            self.stop()
            raise RuntimeError(
                f'брокер не отвечает на {self.host}:{self.port}. Если адрес не '
                'принадлежит этой машине, поправьте [broker] host в stand.ini: '
                'Ватериус ходит на него через NAT точки доступа')

        logger.info(f'брокер слушает {self.host}:{self.port}')

    def stop(self) -> None:
        if self._broker is not None and self._loop is not None:
            try:
                asyncio.run_coroutine_threadsafe(
                    self._broker.shutdown(), self._loop).result(10)
            except Exception as error:                      # noqa: BLE001
                logger.warning(f'брокер не погасился штатно: {error}')
            self._broker = None
        if self._loop is not None:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread is not None:
            self._thread.join(timeout=5)
            self._thread = None
        if self._loop is not None:
            self._loop.close()
            self._loop = None
