"""
Ватериус со стороны стенда: кнопка, входы счётчиков, сброс.

Выдержки не случайны. Механический вход опрашивается раз в 250 мс, замыкание
подтверждается через 50 мс, а конец импульса - три пустых опроса подряд
(Attiny85/src/counter.h). Отсюда 300 мс на замыкание и 800 мс на паузу: короче -
импульс не засчитается, и это не дефект прошивки, а неверное воздействие.

Серия импульсов по возможности выполняется на самой плате (/pulse). Через два
HTTP-запроса на каждый фронт на интервал наматывается RTT 5-50 мс, а тесты
тревог сравнивают интервалы с порогом - там это уже заметно.
"""

from __future__ import annotations

import time
from contextlib import contextmanager
from typing import Any, Iterator

from loguru import logger

# Значения из metf_python_client.boards.esp32c6_super_mini
LOW = 0
HIGH = 1
INPUT = 1
OUTPUT = 3

IMPULSE_WIDTH_MS = 300
IMPULSE_GAP_S = 0.8

BUTTON_SHORT_MS = 100
BUTTON_SETUP_MS = 4000


class Dut:
    """Воздействия на Ватериус через плату METF."""

    def __init__(self, api: Any, button_pin: int, ch0_pin: int, ch1_pin: int,
                 reset_pin: int) -> None:
        self.api = api
        self.button_pin = button_pin
        self.reset_pin = reset_pin
        self._ch = {0: ch0_pin, 1: ch1_pin}

    def init(self) -> None:
        """Все линии в высокоомное состояние: стенд не должен мешать устройству."""
        for pin in (self.button_pin, self.reset_pin, *self._ch.values()):
            self.api.pinMode(pin, INPUT)

    # --- базовое воздействие ---

    def _board_pulse(self, pin: int, value: int, msec: int) -> bool:
        """
        Выдержка на самой плате: `POST /pulse` (protocol v4).

        Клиент 0.4 этого метода не знает, а прошивка умеет, поэтому зовём
        эндпоинт напрямую. Разница принципиальная: короткий импульс, отмеренный
        с компьютера, длится столько, сколько шли два запроса - десятки
        миллисекунд вместо заказанной единицы, - и проверка электронного входа
        на нём проверяла бы совсем не то.
        """
        root = getattr(self.api, '_root', None)
        session = getattr(self.api, '_sess', None)
        if not root or session is None:
            return False
        answer = session.post(f'{root}/pulse',
                              data={'pin': pin, 'value': value, 'duration_ms': msec},
                              timeout=max(5.0, msec / 1000.0 + 5.0))
        answer.raise_for_status()
        return True

    def _low(self, pin: int, msec: int) -> None:
        """Прижать к земле и отпустить в высокоомное состояние."""
        if self._board_pulse(pin, LOW, msec):
            return
        self.api.pinMode(pin, OUTPUT)
        self.api.digitalWrite(pin, LOW)
        time.sleep(msec / 1000.0)
        self.api.pinMode(pin, INPUT)

    # --- кнопка ---

    def press_button(self) -> None:
        """Короткое нажатие - разовая передача показаний."""
        self._low(self.button_pin, BUTTON_SHORT_MS)

    def hold_button(self, msec: int = BUTTON_SETUP_MS) -> None:
        """
        Длинное нажатие - режим настройки. На Ватериусе 2 длительность меряет
        сама ЕСП и переводит attiny в режим настройки при удержании дольше 3 с.
        """
        self._low(self.button_pin, msec)

    def reset(self) -> None:
        self._low(self.reset_pin, BUTTON_SHORT_MS)

    # --- импульсы счётчиков ---

    def pulse(self, channel: int, count: int = 1,
              width_ms: int = IMPULSE_WIDTH_MS, gap: float = IMPULSE_GAP_S) -> None:
        """Подать импульсы подряд с минимально допустимыми выдержками."""
        pin = self._ch[channel]
        for i in range(count):
            self._low(pin, width_ms)
            if i != count - 1:
                time.sleep(gap)

    def pulses(self, channel: int, count: int, gap: float,
               width_ms: int = IMPULSE_WIDTH_MS) -> None:
        """
        Импульсы с заданным интервалом между началами.

        Интервал отсчитывается от абсолютного дедлайна, а не sleep(gap): иначе
        за восемь импульсов набегает полсекунды, а тест непрерывного расхода
        сравнивает ритм с порогом.
        """
        pin = self._ch[channel]
        start = time.time()
        for i in range(count):
            target = start + i * gap
            delay = target - time.time()
            if delay > 0:
                time.sleep(delay)
            self._low(pin, width_ms)

    def pulses_every(self, channel: int, interval: float, minutes: float,
                     width_ms: int = IMPULSE_WIDTH_MS) -> int:
        """Ритмичный расход: импульс раз в interval секунд в течение minutes минут."""
        count = max(1, int(minutes * 60 / interval))
        logger.info(f'канал {channel}: {count} импульсов раз в {interval} с')
        self.pulses(channel, count, interval, width_ms)
        return count

    # --- электронный выход с положительным импульсом ---

    @contextmanager
    def driven(self, channel: int) -> Iterator[None]:
        """
        Взять линию входа под управление: покой - земля, а не высокоомное
        состояние.

        Нужно для типа «Электронный (+)»: подтяжку attiny в нём выключает
        (`counter.h`, set_type), уровень задаёт счётчик, и в покое линию
        обязан держать кто-то. Отпущенная линия у высокоомного входа ловит
        эфир, и импульсы возьмутся из ниоткуда.

        На выходе линия отпускается: для остальных типов входа стенд не
        должен мешать устройству.
        """
        pin = self._ch[channel]
        self.api.pinMode(pin, OUTPUT)
        self.api.digitalWrite(pin, LOW)
        try:
            yield
        finally:
            self.api.pinMode(pin, INPUT)

    def pulse_high(self, channel: int, count: int = 1,
                   width_ms: int = IMPULSE_WIDTH_MS,
                   gap: float = IMPULSE_GAP_S) -> None:
        """
        Импульс подъёмом линии - то, что делает счётчик с положительным
        выходом. Звать только внутри `driven`: между импульсами линия обязана
        оставаться прижатой к земле.
        """
        pin = self._ch[channel]
        for i in range(count):
            self.api.digitalWrite(pin, HIGH)
            time.sleep(width_ms / 1000.0)
            self.api.digitalWrite(pin, LOW)
            if i != count - 1:
                time.sleep(gap)

    # --- датчик протечки ---

    def wet(self, channel: int, closed: bool) -> None:
        """
        Замкнуть или отпустить вход, настроенный как датчик протечки.

        У нормально-разомкнутого датчика тревога - замыкание, у нормально
        замкнутого - размыкание, поэтому здесь просто удержание уровня, а
        смысл задаёт тип входа.
        """
        pin = self._ch[channel]
        if closed:
            self.api.pinMode(pin, OUTPUT)
            self.api.digitalWrite(pin, LOW)
        else:
            self.api.pinMode(pin, INPUT)
