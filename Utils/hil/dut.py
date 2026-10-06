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

Форму сигнала отмеряет плата: серия уезжает одной пачкой (`wave`), и интервалы
внутри неё точны. Через два запроса на каждый фронт к интервалу приклеивалась
дорога по радио - от десятков миллисекунд до двух секунд, - и выдержки
приходилось брать с запасом вслепую. Стенду остаются паузы в десятки секунд: в
них он всё равно вычитывает лог устройства.
"""

from __future__ import annotations

import time
from collections.abc import Callable, Iterator, Mapping, Sequence
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any

from loguru import logger

# Значения из metf_python_client.boards.esp32c6_super_mini
LOW = 0
HIGH = 1
INPUT = 1
OUTPUT = 3

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
# Потолки пачки на плате (metf, `src/wave.h`): участков на линию и длительность
# всей пачки. Что не влезло - серия, и её ведёт стенд
WAVE_MAX_EDGES = 16
# Замыканий в одной пачке: k замыканий занимают 2k-1 участок (k низких и k-1
# пауз между ними), поэтому предел платы в 16 участков - это восемь импульсов
WAVE_MAX_PULSES = (WAVE_MAX_EDGES + 1) // 2
WAVE_MAX_MS = 30000

BUTTON_SHORT_MS = 500
# Режим настройки - удержание от 5100 мс (правило владельца устройства). Порог в
# прошивке ниже - `on_pressed_ms > 3000` (ESP8266/src/wleds.cpp), - но ЕСП считает
# не от начала нажатия, а с той секунды, как сама включилась: attiny замечает
# кнопку своим тактом, потом поднимает питание, и 4000 мс попадали в серую зону
# между порогом в коде и правилом владельца
BUTTON_SETUP_MS = 5100

# Индикатор METF: зелёный горит ровно пока стенд держит линию кнопки прижатой.
# Нужен не тестам, а человеку у стенда: если прогон убили посреди нажатия,
# никакой teardown уже не отработает, и оставшийся гореть светодиод -
# единственный способ это увидеть. Цвет - RRGGBB, как в примерах METF.
# Как часто вычитывать лог в паузе между импульсами и за сколько до импульса
# перестать: опрос платы стоит около 15 мс, а ритм импульсов сравнивается с
# порогом тревоги
PUMP_INTERVAL_S = 1.0
PUMP_TAIL_S = 0.25

LED_PRESSED = '00FF00'
LED_OFF = '000000'
LED_BRIGHTNESS = 40


# attiny видит кнопку на ближайшем такте (button.h, ButtonB2; watchdog до ~275 мс)
# и включает ЕСП через миллисекунды записи EEPROM (main.cpp). Полсекунды - с запасом
WAKE_MARGIN_MS = 500


@dataclass
class Series:
    """Серия с нажатием посреди неё по расписке платы, в мс её аптайма."""

    closures: dict[int, list[tuple[int, int]]]   # канал -> (начало, конец) импульсов
    press: tuple[int, int]

    def awake(self, channel: int, length_ms: int) -> list[tuple[int, int]]:
        """
        Импульсы канала, целиком лёгшие в сеанс ЕСП длиной `length_ms` по её меткам.

        ЕСП включается не раньше нажатия, а метки идут с её включения, поэтому
        `нажатие + длина` - не позже настоящего конца сеанса.
        """
        start = self.press[0] + WAKE_MARGIN_MS
        end = self.press[0] + length_ms
        return [c for c in self.closures[channel] if start <= c[0] and c[1] <= end]


class Dut:
    """Воздействия на Ватериус через плату METF."""

    def __init__(self, api: Any, button_pin: int, ch0_pin: int, ch1_pin: int,
                 reset_pin: int, idle: Callable[[], None] | None = None,
                 settle: Callable[[int], None] | None = None) -> None:
        self.api = api
        self.button_pin = button_pin
        self.reset_pin = reset_pin
        self._ch = {0: ch0_pin, 1: ch1_pin}
        self._led_ok: bool | None = None    # есть ли на плате индикатор
        # Чем заняться в долгой паузе между импульсами: стенд отдаёт сюда
        # вычитывание лога, см. _wait
        self._idle = idle
        # Что сделать перед нажатием: стенд отдаёт сюда выдержку после сеанса
        self._settle = settle

    def init(self) -> None:
        """Все линии в высокоомное состояние: стенд не должен мешать устройству."""
        for pin in (self.button_pin, self.reset_pin, *self._ch.values()):
            self.api.pinMode(pin, INPUT)
        self._led(False)

    def _led(self, pressed: bool) -> None:
        """
        Индикатор нажатия. Плата без светодиода прогон не роняет: это глаза
        человека, а не проверка, - предупреждаем один раз и больше не трогаем.
        """
        if self._led_ok is False:
            return
        try:
            if self._led_ok is None:
                self.api.rgb_begin()
                self.api.rgb_brightness(LED_BRIGHTNESS)
                self._led_ok = True
            self.api.rgb_color(LED_PRESSED if pressed else LED_OFF)
        except Exception as err:
            self._led_ok = False
            logger.warning(f'METF без индикатора ({err}): нажатий видно не будет')

    # --- базовое воздействие ---

    def _board_pulse(self, pin: int, value: int, msec: int) -> bool:
        """
        Выдержка на самой плате: `POST /pulse` (protocol v4).

        Клиент 0.4 этого метода не знает, а прошивка умеет. Разница
        принципиальная: короткий импульс, отмеренный с компьютера, длится
        столько, сколько шли два запроса - десятки миллисекунд вместо
        заказанной единицы, - и проверка электронного входа на нём проверяла бы
        совсем не то.

        Запрос уходит через `Metf.pulse`, а не мимо клиента. Раньше этот метод
        лез в `api._sess` напрямую, и самое частое действие стенда оставалось
        единственным без повторов: девять ошибок прогона пришли отсюда.
        """
        pulse = getattr(self.api, 'pulse', None)
        if pulse is None:
            return False
        pulse(pin, value, msec)
        return True

    def _low(self, pin: int, msec: int) -> bool:
        """
        Прижать к земле и отпустить в высокоомное состояние.

        True - выдержку отмерила плата, и у стенда есть её расписка.
        """
        if self._board_pulse(pin, LOW, msec):
            return True
        self.api.pinMode(pin, OUTPUT)
        self.api.digitalWrite(pin, LOW)
        time.sleep(msec / 1000.0)
        self.api.pinMode(pin, INPUT)
        return False

    # --- кнопка ---

    def _press(self, msec: int) -> None:
        """
        Нажатие с индикацией.

        Гасим только после того, как `_low` вернул управление, то есть линия
        отпущена. Сорвись он на полпути - светодиод останется гореть вместе с
        прижатой линией, а это ровно то, что человеку и надо увидеть.
        """
        if self._settle is not None:
            self._settle(msec)
        вид = 'короткое нажатие' if msec <= BUTTON_SHORT_MS else 'удержание'
        logger.info(f'кнопка: {вид} {msec} мс')
        self._led(True)
        self._low(self.button_pin, msec)
        self._led(False)

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
        высокий уровень, замкнуто. Именно уровень, а не отпускание: внутри
        пачки вывод платы двухтактный и оба уровня выдаются силой
        (`bench_routes.cpp`, pulse_tick). В высокоомное состояние линия уходит
        только по концу пачки, последним моментом расписки.
        """
        return self._pin_line(self._ch[channel], edges, value, at_ms)

    @staticmethod
    def _pin_line(pin: int, edges: Sequence[int], value: int = LOW,
                  at_ms: int = 0) -> dict[str, Any]:
        return {'pin': pin, 'value': value, 'at_ms': at_ms, 'edges': list(edges)}

    def wave(self, *lines: dict[str, Any]) -> None:
        """
        Подать пачку и дождаться её конца.

        Одним запросом, потому что интервалы между фронтами обязана отмерять
        плата: через два запроса на интервал наматывается дорога по радио, и
        заказанные 0,3 с приходили как две секунды.
        """
        self.api.wave(list(lines))

    def moments(self, channel: int) -> list[int]:
        """
        Моменты фронтов последней пачки на этом входе - по часам платы,
        в миллисекундах её аптайма. Их на один больше, чем участков: последний
        момент - отпускание линии.

        Этим тест утверждает о воздействии. Заказ ехал по радио и в дороге
        искажался; расписка снята на плате, и если она разошлась с заказом -
        виноват стенд, а не устройство.
        """
        return self._pin_moments(self._ch[channel])

    def _pin_moments(self, pin: int) -> list[int]:
        stat = self.api.pulse_stat()
        for line in stat.get('lines', []):
            if line.get('pin') == pin:
                return [int(mark) for mark in line['edges_ms']]
        raise AssertionError(f'в расписке платы нет вывода {pin}: {stat}')

    def delivered(self, channel: int) -> list[int]:
        """Длительности участков, которые плата отмерила на самом деле."""
        marks = self.moments(channel)
        return [after - before
                for before, after in zip(marks, marks[1:], strict=False)]

    def pulse(self, channel: int, count: int = 1,
              width_ms: int = IMPULSE_WIDTH_MS, gap: float = IMPULSE_GAP_S) -> int:
        """
        Подать импульсы подряд с минимально допустимыми выдержками.

        Возвращает, сколько замыканий выдала сама плата по своей расписке: этим
        числом тест отделяет «стенд не довёл импульс» от «прошивка не посчитала».

        Длинная серия режется на пачки, а не на запросы. Внутри пачки интервалы
        отмеряет плата, и за стендом остаётся только стык между пачками - вместо
        одного запроса на каждый импульс. Прежде серия длиннее пачки уезжала
        именно так, и один импульс из десяти терялся в радио: отказ D3 «до входа
        дошло 9 импульсов из 10» повторился в двух прогонах подряд, а доказать,
        кто его потерял, было нечем - в одиночку тест проходил.
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
            # Лог устройства во время пачки вычитываем: пачка идёт секундами, а
            # кольцо платы вмещает полтора сеанса
            total_s = self.api.wave([self.line(channel, edges)], wait=False)
            self._wait(time.monotonic() + total_s)
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

        Нажатие едет линией той же пачки, что и импульсы: тогда моменты
        пробуждения и импульсов стоят в одной расписке, по одним часам, и из
        неё видно, какие импульсы пришлись на сеанс. Нажатие отдельным запросом
        затёрло бы расписку пачки.

        Серия длиннее пачки идёт пачками подряд. Каждая начинается с паузы
        `step_ms` минус самый поздний конец импульса в шаге: задержка радио
        между пачками только удлиняет паузу и не сливает два замыкания в одно.

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
        # В начале паузы: следующий импульс встаёт на всю паузу позже нажатия
        # и успевает за WAKE_MARGIN_MS, а не теряется у края сеанса
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
                lines.append(self._pin_line(self.button_pin, [BUTTON_SHORT_MS],
                                            at_ms=press_at))
            total_s = self.api.wave(lines, wait=False)
            self._wait(time.monotonic() + total_s)

            for ch in shapes:
                marks = self.moments(ch)
                closures[ch] += list(zip(marks[0::2], marks[1::2], strict=False))
            if press is None:
                marks = self._pin_moments(self.button_pin)
                press = (marks[0], marks[1])
            left -= k
        assert press is not None
        return Series(closures=closures, press=press)

    def impulses_delivered(self, channel: int) -> int:
        """
        Сколько замыканий плата выдала в последней пачке - по её расписке.

        Замыкание - это участок низкого уровня, а моментов в расписке на один
        больше, чем участков, поэтому замыканий ровно половина моментов.
        """
        return len(self.moments(channel)) // 2

    def pulses(self, channel: int, count: int, gap: float,
               width_ms: int = IMPULSE_WIDTH_MS) -> int:
        """
        Импульсы с заданным интервалом между началами. Возвращает, сколько
        замыканий выдала плата по своей расписке.

        Интервал отсчитывается от абсолютного дедлайна, а не sleep(gap): иначе
        за восемь импульсов набегает полсекунды, а тест непрерывного расхода
        сравнивает ритм с порогом. Пачкой такую серию не закажешь - её интервалы
        нарочно длиннее сеанса, - поэтому расписку снимаем после каждого
        замыкания: иначе «до входа дошло меньше» не отличить от «стенд не довёл».

        Догоняя расписание после осечки связи с платой, замыкание не встаёт
        ближе IMPULSE_GAP_S к предыдущему: иначе два замыкания уходят вплотную и
        attiny честно считает их одним. D7 30 сентября так недосчитался двух
        импульсов из десяти после четырёхсекундной отлучки METF.
        """
        pin = self._ch[channel]
        start = time.monotonic()
        подано = 0
        неподтверждённых = 0
        свободна = start - IMPULSE_GAP_S      # когда линия отпущена в последний раз
        отстало = 0.0
        for i in range(count):
            срок = start + i * gap
            self._wait(max(срок, свободна + IMPULSE_GAP_S))
            отстало = max(отстало, time.monotonic() - срок)
            подтверждено = self._low(pin, width_ms)
            свободна = time.monotonic()
            if not подтверждено:
                # Плата без `/pulse` вообще не даёт расписок. На собранном стенде
                # этого не бывает: протокол 14 требует досмотр перед прогоном
                подано += 1
                неподтверждённых += 1
                continue
            # Расписку платы перекрывает любая следующая пачка, в том числе
            # нажатие кнопки из другого потока (`/pulse/stat` помнит последнюю).
            # Тогда доказательства нет, но расписка 202 есть: плата приняла
            # заказ и завела таймер
            try:
                подано += self.impulses_delivered(channel)
            except AssertionError:
                подано += 1
                неподтверждённых += 1
        if неподтверждённых:
            logger.warning(
                f'расписка платы не подтвердила {неподтверждённых} замыканий из '
                f'{count} на входе {channel}: её перекрыла другая пачка. Плата '
                f'заказ приняла (202), но доказательства выдачи на эти нет')
        if отстало > PUMP_INTERVAL_S:
            logger.warning(
                f'серия на входе {channel} отстала от расписания до {отстало:.1f} с: '
                f'связь с платой медлила. Паузы между замыканиями при этом не '
                f'короче {IMPULSE_GAP_S} с, ритм серии сбит')
        return подано

    def _wait(self, target: float) -> None:
        """
        Дождаться момента target, не оставляя лог устройства без присмотра.

        Пауза между импульсами - это минуты, а кольцо METF вмещает полтора
        сеанса (511 строк). Устройство за такую паузу успевает проснуться по
        расписанию и напечатать сотни строк, и пока пауза была одним sleep,
        кольцо переполнялось: тест падал не на своей проверке, а на «лог
        неполон, утверждать по нему нечего».

        Последние PUMP_TAIL_S до импульса не вычитываем: один опрос платы стоит
        десяток миллисекунд, а ритм импульсов в тестах расхода сравнивается с
        порогом. Дедлайн абсолютный, так что съеденное время не накапливается.
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
                   gap: float = IMPULSE_GAP_S) -> int:
        """
        Импульс подъёмом линии - то, что делает счётчик с положительным
        выходом. Звать только внутри `driven`: между импульсами линия обязана
        оставаться прижатой к земле.

        Возвращает, сколько импульсов выдала сама плата по своей расписке - тем
        же числом, что и `pulse`, и по той же причине: без расписки «до входа
        дошло меньше» не отличить от «стенд не довёл». Здесь это было особенно
        дорого - фронты набирал компьютер двумя запросами на импульс, и в
        тридцатимиллисекундный импульс укладывалась дорога по радио, а осечка
        связи добавляла туда же паузу повтора.

        Пачка заказывается с хвостовой паузой, поэтому кончается низким
        уровнем: плата отпускает линию последним моментом, и отпустить её надо
        с уровня покоя, иначе высокоомный вход поймает эфир. Уровень покоя
        после пачки восстанавливает `driven`, к которой возвращается управление.
        """
        pin = self._ch[channel]
        gap_ms = int(gap * 1000)
        # Участков в пачке вдвое больше импульсов: на каждый подъём своя пауза
        # следом. Предел платы в участках тот же, что у замыканий
        шаг = width_ms + gap_ms
        в_пачке = max(1, min(WAVE_MAX_PULSES, WAVE_MAX_MS // шаг))
        подано = 0
        осталось = count
        while осталось:
            сколько = min(осталось, в_пачке)
            edges = [width_ms, gap_ms] * сколько
            total_s = self.api.wave(
                [self.line(channel, edges, value=HIGH)], wait=False)
            self._wait(time.monotonic() + total_s)
            подано += self.impulses_delivered(channel)
            осталось -= сколько
            # Линию плата отпустила: вернуть её в покой до следующей пачки
            self.api.pinMode(pin, OUTPUT)
            self.api.digitalWrite(pin, LOW)
        return подано

    # --- датчик протечки ---

    def wet(self, channel: int, closed: bool) -> None:
        """
        Замкнуть или отпустить вход, настроенный как датчик протечки.

        У нормально-разомкнутого датчика тревога - замыкание, у нормально
        замкнутого - размыкание, поэтому здесь просто удержание уровня, а
        смысл задаёт тип входа.

        Стенд умеет только края диапазона: 0 Ом и высокоомный вход. Настоящая
        вода - это десятки и сотни килоом, и порог на неё
        (`Attiny85/src/wet.h`) проверяется хостовыми тестами и резистором на
        столе, а не отсюда.
        """
        pin = self._ch[channel]
        if closed:
            self.api.pinMode(pin, OUTPUT)
            self.api.digitalWrite(pin, LOW)
        else:
            self.api.pinMode(pin, INPUT)
