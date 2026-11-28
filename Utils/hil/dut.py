"""
Ватериус со стороны стенда: кнопка и входы счётчиков.

Сеанс начинает только кнопка: Ватериус всегда спит, короткое нажатие (от 300 до
4900 мс) поднимает его на передачу, долгое (от 5100 мс) - в режим настройки.
Линию сброса стенд лишь паркует в высокоомном состоянии, а импульсом по ней не
пользуется: замер 27 сентября - 150 с после импульса устройство не сказало ни
строки, хотя счётчик сбросов attiny вырос. Сброс attiny сеанса не начинает, и
ждать его после сброса бессмысленно.

Выдержки не случайны. Механический вход опрашивается раз в 250 мс, замыкание
подтверждается через 50 мс, а конец импульса - три пустых опроса подряд
(Attiny85/src/counter.h). Отсюда импульс стенда: 500 мс замкнуто и 800 мс
разомкнуто - короче замыкание не засчитается или сольётся со следующим, и это
не дефект прошивки, а неверное воздействие.

Форму сигнала отмеряет программатор на гребёнке: серия уезжает одной пачкой (`wave`), и
интервалы внутри неё точны до миллисекунды. Стенду остаются паузы в десятки секунд: в них он
всё равно вычитывает лог устройства.
"""

from __future__ import annotations

import time
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from typing import Any

from loguru import logger

from .programmer import LineBusy

# Уровень первого участка пачки: 0 - линия прижата к земле, 1 - отпущена
LOW = 0
RELEASED = 1

# Линии программатора по ролям (waterius_flasher, спека bench-control)
BUTTON = 'button'
RESET = 'rst'
CHANNELS = {0: 'ch0', 1: 'ch1'}

# Длительность замыкания. Вне сеанса вход опрашивается раз в 250 мс, и замыкание
# attiny подтверждает повторным чтением через 50 мс (counter.h, discrete): худший
# случай - опрос на 250-й миллисекунде, переспрос на 300-й, и 350 мс хватило бы.
#
# Но внутри сеанса такт отмеряется числом оборотов цикла, а не часами
# (main.cpp, counting_1ms), и оборот стоит дороже миллисекунды: чтение АЦП,
# прерывания I2C от ЕСП, 50 мс переспроса, на которые цикл встаёт целиком.
# Период там - «250 мс плюс сколько получится», и на 350 мс замер 26.09 дал
# девять импульсов из десяти (test_D7, приросты 20 + 70 вместо 100). Прошивка
# attiny 46 считает этот такт по millis(), но стенд обязан работать и с
# прежними, поэтому запас держим на самом дорогом такте
IMPULSE_WIDTH_MS = 500

# Пауза между замыканиями. Конец импульса attiny видит по трём разомкнутым
# опросам подряд (counter.h, discrete): при точном такте 250 мс это 750 мс в
# худшей фазе. Но такт отмеряет watchdog attiny, а его генератор гуляет
# процентов на десять, и каждое замыкание вдобавок сдвигает фазу на ~50 мс -
# столько занимает переспрос, подтверждающий замыкание.
#
# Поэтому 800 мс, поставленные по одной арифметике, запаса не имели: замер
# 27 сентября дал 9 замыканий из 10 при доказанной расписке платы (10 по 500 мс
# с паузами ровно 800 мс) - два замыкания слились в одно. Это не дефект прошивки,
# а слишком тесное воздействие: 1,2 с держат запас и на разброс такта, и на фазу
IMPULSE_GAP_S = 1.2

# Классике на короткое нажатие нужен такт WDT (250 мс), заставший кнопку прижатой
# уже после первого обнаружения (button.h: on_time > 10); при любой фазе это
# гарантируют полсекунды. До долгого далеко: 2 с у классики, 3 с у Ватериуса-2
# Потолки пачки программатора (waterius_flasher, `src/core/wave.h`): участков на линию и
# длительность всей пачки. Что не влезло - серия, и её ведёт стенд
WAVE_MAX_EDGES = 32
# Замыканий в одной пачке: k замыканий занимают 2k-1 участок (k низких и k-1
# пауз между ними), поэтому предел в 32 участка - это шестнадцать импульсов
WAVE_MAX_PULSES = (WAVE_MAX_EDGES + 1) // 2
WAVE_MAX_MS = 30000

BUTTON_SHORT_MS = 500
# Режим настройки - удержание от 5100 мс (правило владельца устройства). Порог в
# прошивке ниже - `on_pressed_ms > 3000` (ESP8266/src/wleds.cpp), - но ЕСП считает
# не от начала нажатия, а с той секунды, как сама включилась: attiny замечает
# кнопку своим тактом, потом поднимает питание, и 4000 мс попадали в серую зону
# между порогом в коде и правилом владельца
BUTTON_SETUP_MS = 5100

# Как часто вычитывать лог в паузе между импульсами и за сколько до импульса
# перестать: опрос программатора стоит миллисекунды, а ритм импульсов сравнивается с
# порогом тревоги
PUMP_INTERVAL_S = 1.0
PUMP_TAIL_S = 0.25

# Сколько уборка ждёт конца идущей пачки: дольше пачка не бывает
RELEASE_WAIT_S = WAVE_MAX_MS / 1000 + 1


@dataclass
class Series:
    """Серия с нажатием посреди неё по расписке программатора, в мс с его пуска."""

    closures: dict[int, list[tuple[int, int]]]   # канал -> (начало, конец) импульсов
    press: tuple[int, int]


class Dut:
    """Воздействия на Ватериус через программатор на гребёнке (`programmer.Programmer`)."""

    def __init__(self, api: Any, idle: Callable[[], None] | None = None,
                 settle: Callable[[int], None] | None = None) -> None:
        self.api = api
        # Чем заняться в долгой паузе между импульсами: стенд отдаёт сюда
        # вычитывание лога, см. _wait
        self._idle = idle
        # Что сделать перед нажатием: стенд отдаёт сюда выдержку после сеанса
        self._settle = settle

    def init(self) -> None:
        """
        Все линии отпущены: стенд не должен мешать устройству.

        Не `#lines off`: он закрывает приём лога, и строки сеанса, идущего во время уборки,
        пропали бы. Удержания снимаются `release`, а линию с идущей пачкой отпустит конец
        пачки - его и ждём.
        """
        deadline = time.monotonic() + RELEASE_WAIT_S
        for line in (BUTTON, RESET, *CHANNELS.values()):
            while True:
                try:
                    self.api.release(line)
                    break
                except LineBusy:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.5)

    # --- базовое воздействие ---

    def _low(self, line: str, msec: int) -> None:
        """Прижать к земле на msec и отпустить: пачка из одного участка."""
        total_ms = self.api.wave([self._line_order(line, [msec])])
        time.sleep(total_ms / 1000.0)

    # --- кнопка ---

    def _press(self, msec: int) -> None:
        if self._settle is not None:
            self._settle(msec)
        вид = 'короткое нажатие' if msec <= BUTTON_SHORT_MS else 'удержание'
        logger.info(f'кнопка: {вид} {msec} мс')
        self._low(BUTTON, msec)

    def press_button(self) -> None:
        """Короткое нажатие - разовая передача показаний."""
        self._press(BUTTON_SHORT_MS)

    def hold_button(self, msec: int = BUTTON_SETUP_MS) -> None:
        """
        Длинное нажатие - режим настройки. На Ватериусе 2 длительность меряет
        сама ЕСП и переводит attiny в режим настройки при удержании дольше 3 с.
        """
        self._press(msec)

    # --- импульсы счётчиков ---

    def line(self, channel: int, edges: Sequence[int], value: int = LOW,
             at_ms: int = 0) -> dict[str, Any]:
        """
        Линия пачки: длительности участков в миллисекундах, уровень первого из
        них, смещение начала от общего старта пачки.

        Уровни чередуются: [500, 300, 500] при value=LOW - это замкнуто,
        отпущено, замкнуто. «Отпущено» - высокоомное состояние, как у разомкнутого
        геркона: в 1 программатор линии не выводит. После последнего участка линия
        отпущена, это последний момент расписки.
        """
        return self._line_order(CHANNELS[channel], edges, value, at_ms)

    @staticmethod
    def _line_order(line: str, edges: Sequence[int], value: int = LOW,
                    at_ms: int = 0) -> dict[str, Any]:
        return {'line': line, 'value': value, 'at_ms': at_ms, 'edges': list(edges)}

    def wave(self, *lines: dict[str, Any]) -> None:
        """Подать пачку и дождаться её конца."""
        total_ms = self.api.wave(list(lines))
        time.sleep(total_ms / 1000.0)

    def moments(self, channel: int) -> list[int]:
        """
        Моменты фронтов последней пачки на этом входе - по часам программатора,
        в миллисекундах с его пуска. Их на один больше, чем участков: последний
        момент - отпускание линии.

        Этим тест утверждает о воздействии: если расписка разошлась с заказом -
        виноват стенд, а не устройство.
        """
        return self._line_moments(CHANNELS[channel])

    def _line_moments(self, line: str) -> list[int]:
        stat = self.api.wave_stat()
        for order in stat.get('lines', []):
            if order.get('line') == line:
                return [int(mark) for mark in order['edges_ms']]
        raise AssertionError(f'в расписке программатора нет линии {line}: {stat}')

    def delivered(self, channel: int) -> list[int]:
        """Длительности участков, которые программатор отмерил на самом деле."""
        marks = self.moments(channel)
        return [after - before
                for before, after in zip(marks, marks[1:], strict=False)]

    def pulse(self, channel: int, count: int = 1,
              width_ms: int = IMPULSE_WIDTH_MS, gap: float = IMPULSE_GAP_S) -> int:
        """
        Подать импульсы подряд с минимально допустимыми выдержками.

        Возвращает, сколько замыканий выдал программатор по своей расписке: этим
        числом тест отделяет «стенд не довёл импульс» от «прошивка не посчитала».

        Длинная серия режется на пачки, а не на команды: внутри пачки интервалы
        отмеряет программатор, и за стендом остаётся только стык между пачками.
        """
        подано = 0
        осталось = count
        # Сколько замыканий влезает в одну пачку: k замыканий занимают 2k-1
        # участок и длятся k*(замыкание+пауза) - пауза. Длинная пауза (тесты
        # расхода заказывают минуты) не влезает вовсе, и тогда пачка - одно
        # замыкание: интервал остаётся за стендом, зато расписка есть на каждое
        шаг = width_ms + int(gap * 1000)
        в_пачке = max(1, min(WAVE_MAX_PULSES,
                             (WAVE_MAX_MS + int(gap * 1000)) // шаг))
        while осталось:
            сколько = min(осталось, в_пачке)
            edges = [width_ms]
            for _ in range(сколько - 1):
                edges += [int(gap * 1000), width_ms]
            # Лог устройства во время пачки вычитываем: пачка идёт секундами
            total_ms = self.api.wave([self.line(channel, edges)])
            self._wait(time.monotonic() + total_ms / 1000.0)
            подано += self.impulses_delivered(channel)
            осталось -= сколько
            if осталось:
                self._wait(time.monotonic() + gap)
        return подано

    def series(self, shapes: Mapping[int, tuple[int, int]], count: int,
               step_ms: int, press_after: int,
               min_gap_ms: int = int(IMPULSE_GAP_S * 1000)) -> Series:
        """
        Серия импульсов, внутри которой стенд жмёт кнопку.

        Нажатие едет линией той же пачки, что и импульсы: отмеряет его программатор, а
        нажатие отдельной командой затёрло бы расписку пачки.

        Серия длиннее пачки идёт пачками подряд. Каждая начинается с паузы
        `step_ms` минус самый поздний конец импульса в шаге: задержка между
        пачками только удлиняет паузу и не сливает два замыкания в одно.

        @param shapes  канал -> (длина импульса, смещение от начала шага), мс
        @param count   импульсов на каждый канал
        @param step_ms шаг между началами импульсов
        @param press_after сколько импульсов подать до нажатия
        @param min_gap_ms  кратчайшая пауза, которую вход отличит от импульса:
                           по умолчанию - механического входа
        """
        widest = max(width + offset for width, offset in shapes.values())
        lead = step_ms - widest
        if lead < min_gap_ms:
            raise ValueError(f'пауза между импульсами {lead} мс короче '
                             f'{min_gap_ms}: импульсы сольются')
        per_wave = min(WAVE_MAX_PULSES,
                       (WAVE_MAX_MS - lead - widest) // step_ms + 1)
        if not 0 <= press_after < min(count, per_wave):
            raise ValueError(f'нажатие после {press_after}-го импульса не '
                             f'ложится в первую пачку из {min(count, per_wave)}')

        if self._settle is not None:
            self._settle(BUTTON_SHORT_MS)
        # Сразу после отпускания импульса, в паузе: нажатие не накрывает импульс
        press_at = press_after * step_ms + 1
        logger.info(f'кнопка: короткое нажатие {BUTTON_SHORT_MS} мс внутри '
                    f'серии, после {press_after}-го импульса')

        closures: dict[int, list[tuple[int, int]]] = {ch: [] for ch in shapes}
        press: tuple[int, int] | None = None
        left = count
        while left:
            k = min(left, per_wave)
            lines = []
            for ch, (width, offset) in shapes.items():
                edges = [width]
                for _ in range(k - 1):
                    edges += [step_ms - width, width]
                lines.append(self.line(ch, edges, at_ms=lead + offset))
            if press is None:
                lines.append(self._line_order(BUTTON, [BUTTON_SHORT_MS], at_ms=press_at))
            total_ms = self.api.wave(lines)
            self._wait(time.monotonic() + total_ms / 1000.0)

            for ch in shapes:
                marks = self.moments(ch)
                closures[ch] += list(zip(marks[0::2], marks[1::2], strict=False))
            if press is None:
                marks = self._line_moments(BUTTON)
                press = (marks[0], marks[1])
            left -= k
        assert press is not None
        return Series(closures=closures, press=press)

    def impulses_delivered(self, channel: int) -> int:
        """
        Сколько замыканий программатор выдал в последней пачке - по его расписке.

        Замыкание - это участок низкого уровня, а моментов в расписке на один
        больше, чем участков, поэтому замыканий ровно половина моментов.
        """
        return len(self.moments(channel)) // 2

    def pulses(self, channel: int, count: int, gap: float,
               width_ms: int = IMPULSE_WIDTH_MS) -> int:
        """
        Импульсы с заданным интервалом между началами. Возвращает, сколько
        замыканий выдал программатор по своей расписке.

        Интервал отсчитывается от абсолютного дедлайна, а не sleep(gap): иначе
        за восемь импульсов набегает полсекунды, а тест непрерывного расхода
        сравнивает ритм с порогом. Пачкой такую серию не закажешь - её интервалы
        нарочно длиннее сеанса, - поэтому расписку снимаем после каждого
        замыкания: иначе «до входа дошло меньше» не отличить от «стенд не довёл».

        Догоняя расписание после задержки, замыкание не встаёт ближе IMPULSE_GAP_S
        к предыдущему: иначе два замыкания уходят вплотную и attiny честно считает
        их одним.
        """
        line = CHANNELS[channel]
        start = time.monotonic()
        подано = 0
        свободна = start - IMPULSE_GAP_S      # когда линия отпущена в последний раз
        отстало = 0.0
        for i in range(count):
            срок = start + i * gap
            self._wait(max(срок, свободна + IMPULSE_GAP_S))
            отстало = max(отстало, time.monotonic() - срок)
            self._low(line, width_ms)
            свободна = time.monotonic()
            подано += self.impulses_delivered(channel)
        if отстало > PUMP_INTERVAL_S:
            logger.warning(
                f'серия на входе {channel} отстала от расписания до {отстало:.1f} с. '
                f'Паузы между замыканиями при этом не короче {IMPULSE_GAP_S} с, ритм '
                f'серии сбит')
        return подано

    def _wait(self, target: float) -> None:
        """
        Дождаться момента target, не оставляя лог устройства без присмотра.

        Пауза между импульсами - это минуты, а кольцо программатора вмещает секунды
        самого плотного лога. Устройство за такую паузу успевает проснуться по
        расписанию и напечатать сотни строк, и одним sleep кольцо переполнилось бы:
        тест падал бы не на своей проверке, а на «лог неполон».

        Последние PUMP_TAIL_S до импульса не вычитываем: опрос стоит миллисекунды,
        а ритм импульсов в тестах расхода сравнивается с порогом. Дедлайн
        абсолютный, так что съеденное время не накапливается.
        """
        if self._idle is None:
            delay = target - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            return

        while True:
            delay = target - time.monotonic()
            if delay <= PUMP_TAIL_S:
                if delay > 0:
                    time.sleep(delay)
                return
            self._idle()
            # Ещё раз: опрос занял время, и до импульса могло остаться меньше
            delay = min(PUMP_INTERVAL_S, target - time.monotonic() - PUMP_TAIL_S)
            if delay > 0:
                time.sleep(delay)

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
        Замкнуть или отпустить вход, настроенный как датчик протечки или сигнализация.

        У датчика протечки тревога - замыкание, у сигнализации - размыкание,
        поэтому здесь просто удержание уровня, а смысл задаёт тип входа.

        Стенд умеет только края диапазона: 0 Ом и высокоомный вход. Настоящая
        вода - это десятки и сотни килоом, и порог на неё
        (`Attiny85/src/wet.h`) проверяется хостовыми тестами и резистором на
        столе, а не отсюда.
        """
        line = CHANNELS[channel]
        if closed:
            self.api.hold(line)
        else:
            self.api.release(line)
