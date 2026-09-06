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
from typing import Any

from loguru import logger

# Значения из metf_python_client.boards.esp32c6_super_mini
LOW = 0
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

    def _low(self, pin: int, msec: int) -> None:
        """Прижать к земле и отпустить в высокоомное состояние."""
        try:
            self.api.pulse(pin, LOW, msec)          # клиент 0.4: выдержка на плате
        except AttributeError:
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
