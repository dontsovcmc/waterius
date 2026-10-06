"""
Паузы между импульсами не оставляют лог устройства без присмотра.

Железо не нужно: плату заменяет заглушка, а часами управляет тест. Проверяется
то, из-за чего падал E2: пока пауза была одним `sleep`, устройство успевало
проснуться по расписанию, напечатать сотни строк, и кольцо METF (511 строк,
полтора сеанса) переполнялось. Тест падал не на своей проверке, а на «лог
неполон, утверждать по нему нечего».
"""

from __future__ import annotations

import pytest

from .. import dut as dut_mod
from ..dut import BUTTON_SETUP_MS, BUTTON_SHORT_MS, WAKE_MARGIN_MS, Series


class Clock:
    """Часы под управлением теста: `sleep` двигает время, а не ждёт."""

    def __init__(self) -> None:
        self.now = 1000.0

    def time(self) -> float:
        return self.now

    def monotonic(self) -> float:
        """Стенд отмеряет сроки монотонными часами: под тестом они те же."""
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeApi:
    """Плата, которая молча принимает всё: нас интересуют только паузы."""

    def __init__(self, clock: Clock) -> None:
        self.clock = clock
        self.lows: list[float] = []       # когда прижималась линия

    def pinMode(self, pin: int, mode: int) -> None:      # noqa: N802
        pass

    def digitalWrite(self, pin: int, value: int) -> None:  # noqa: N802
        if value == dut_mod.LOW:
            self.lows.append(self.clock.now)

    def __getattr__(self, name: str):                    # rgb_*, pulse и прочее
        raise AttributeError(name)


@pytest.fixture
def clock(monkeypatch: pytest.MonkeyPatch) -> Clock:
    fake = Clock()
    monkeypatch.setattr(dut_mod, 'time', fake)
    return fake


def make(clock: Clock, idle=None, settle=None) -> tuple[dut_mod.Dut, FakeApi]:
    api = FakeApi(clock)
    board = dut_mod.Dut(api, button_pin=1, ch0_pin=2, ch1_pin=3, reset_pin=4,
                        idle=idle, settle=settle)
    board._led_ok = False                 # индикатора у заглушки нет
    return board, api


def test_без_расписки_серия_считается_по_заказу(clock: Clock) -> None:
    """
    Плата без `/pulse` расписки не даёт, и доказать доставку нечем. На собранном
    стенде такого не бывает - протокол 14 требует досмотр перед прогоном, - но
    падать на этом стенд не должен.
    """
    board, api = make(clock)

    подано = board.pulses(channel=0, count=3, gap=1.0)

    assert подано == 3
    assert len(api.lows) == 3


def test_пауза_вычитывает_лог(clock: Clock) -> None:
    """Минута между импульсами - это десятки обращений к плате, а не одно."""
    reads: list[float] = []
    board, _ = make(clock, idle=lambda: reads.append(clock.now))

    board.pulses(channel=0, count=2, gap=60.0, width_ms=500)

    assert len(reads) >= 50, (
        f'за минуту паузы лог вычитан {len(reads)} раз: кольцо METF вмещает '
        'полтора сеанса и переполнится плановым пробуждением')


def test_ритм_импульсов_не_плывёт(clock: Clock) -> None:
    """
    Вычитывание не имеет права сдвинуть импульс: ритм сравнивается с порогом
    тревоги. Дедлайн абсолютный, поэтому съеденное время не накапливается.
    """
    board, api = make(clock, idle=lambda: clock.sleep(0.015))  # опрос платы

    board.pulses(channel=0, count=4, gap=60.0, width_ms=500)

    starts = api.lows
    assert len(starts) == 4
    for i, moment in enumerate(starts):
        drift = abs(moment - (starts[0] + i * 60.0))
        assert drift < 0.5, f'импульс {i} уехал на {drift:.3f} с'


def test_догоняя_расписание_серия_не_сливает_замыкания(clock: Clock) -> None:
    """
    Осечка связи с платой посреди серии: следующие сроки уже в прошлом, но
    замыкание всё равно не встаёт вплотную к предыдущему - attiny слил бы их
    в один импульс (D7, 30 сентября: 8 из 10).
    """
    board, api = make(clock)
    отлучка = {'после': 2, 'на': 4.0}
    прижать = api.digitalWrite

    def с_отлучкой(pin: int, value: int) -> None:
        if value == dut_mod.LOW and len(api.lows) == отлучка['после']:
            clock.sleep(отлучка['на'])              # плата не отвечала
        прижать(pin, value)

    api.digitalWrite = с_отлучкой                  # type: ignore[method-assign]

    board.pulses(channel=1, count=5, gap=2.0, width_ms=500)

    starts = api.lows
    assert len(starts) == 5
    for before, after in zip(starts, starts[1:], strict=False):
        assert after - (before + 0.5) >= dut_mod.IMPULSE_GAP_S - 1e-9, (
            f'пауза {after - before - 0.5:.2f} с: замыкания сольются')


def test_без_вычитывателя_ведёт_себя_как_раньше(clock: Clock) -> None:
    """Dut портала создаётся без лога - для него ничего не изменилось."""
    board, api = make(clock, idle=None)

    board.pulses(channel=0, count=2, gap=60.0, width_ms=500)

    assert [round(t - api.lows[0]) for t in api.lows] == [0, 60]


def test_выдержка_идёт_до_нажатия(clock: Clock) -> None:
    """
    Нажатие вплотную к концу сеанса до устройства не доходит (механизм не
    установлен, долг в README). Выдержку отмеряет стенд, но ждать она обязана
    до того, как линия прижата, - иначе ждать уже нечего.
    """
    было: list[str] = []
    board, api = make(clock, settle=lambda _: было.append(f'выдержка {clock.now}'))

    board.press_button()

    assert было, 'выдержки перед нажатием не было'
    прижали = api.lows[0]
    выждали = float(было[0].split()[1])
    assert выждали <= прижали, 'выдержка кончилась позже, чем прижали линию'


def test_без_выдержки_нажатие_работает_как_прежде(clock: Clock) -> None:
    """Стенд может не давать выдержки вовсе: тогда нажатие идёт сразу."""
    board, _ = make(clock)
    board.press_button()                # не должно упасть


def test_выдержка_знает_длительность_нажатия(clock: Clock) -> None:
    """
    Длинным нажатием открывают портал, коротким заказывают сеанс, и стенду
    надо их различать: приговор «не проснулся за 2 с» относится только ко
    второму. Раньше выдержка не знала, какое нажатие идёт, и стенд мерил
    двумя секундами открытие портала - так падал test_E19.
    """
    длительности: list[int] = []
    board, _ = make(clock, settle=длительности.append)

    board.press_button()
    board.hold_button()

    assert длительности == [BUTTON_SHORT_MS, BUTTON_SETUP_MS], длительности


class WavingApi(FakeApi):
    """
    Плата, умеющая пачку: запоминает заказ и отдаёт по нему расписку так, как
    отдала бы настоящая - моментами фронтов по своим часам.
    """

    def __init__(self, clock: Clock) -> None:
        super().__init__(clock)
        self.waves: list[list[dict]] = []
        self._marks: dict[int, list[int]] = {}

    def wave(self, lines: list[dict], wait: bool = True) -> float:
        self.waves.append(lines)
        start = int(self.clock.now * 1000)
        longest = 0.0
        for line in lines:
            at = line.get('at_ms', 0)
            marks = [start + at]
            for edge in line['edges']:
                marks.append(marks[-1] + edge)
            self._marks[line['pin']] = marks
            longest = max(longest, (marks[-1] - start) / 1000.0)
        if wait:
            self.clock.sleep(longest)
        return longest

    def pulse_stat(self) -> dict:
        return {'lines': [{'pin': pin, 'edges_ms': marks}
                          for pin, marks in self._marks.items()]}


def waving(clock: Clock) -> tuple[dut_mod.Dut, WavingApi]:
    api = WavingApi(clock)
    board = dut_mod.Dut(api, button_pin=1, ch0_pin=2, ch1_pin=3, reset_pin=4)
    board._led_ok = False
    return board, api


def test_серия_уходит_одной_пачкой(clock: Clock) -> None:
    """
    Интервалы внутри серии обязана отмерять плата: через два запроса на каждый
    фронт к паузе приклеивается дорога по радио, и заказанные 0,3 с приходили
    как две секунды.
    """
    board, api = waving(clock)

    подано = board.pulse(channel=0, count=3, width_ms=500, gap=1.2)

    assert len(api.waves) == 1, f'пачек {len(api.waves)}, а нужна одна'
    assert api.waves[0][0]['edges'] == [500, 1200, 500, 1200, 500]
    assert not api.lows, 'линию дёргал стенд, хотя пачка уехала на плату'
    assert подано == 3, f'по расписке платы замыканий {подано}, а заказано 3'


def test_длинная_пауза_режет_серию_на_пачки_по_одному(clock: Clock) -> None:
    """
    Пачка на плате не длиннее полминуты, и это не произвол: в длинных паузах
    стенд вычитывает лог устройства, иначе кольцо платы переполняется. Но
    замыкание всё равно заказывается пачкой - иначе у стенда нет расписки, и
    «до входа дошло 9 импульсов из 10» не отличить от «стенд не довёл».
    """
    board, api = waving(clock)

    подано = board.pulse(channel=0, count=2, width_ms=500, gap=60.0)

    assert [line[0]['edges'] for line in api.waves] == [[500], [500]], (
        f'минутную паузу отдали плате: {api.waves}')
    assert not api.lows, 'линию дёргал стенд, хотя расписку даёт только пачка'
    assert подано == 2


def test_расписка_даёт_фактические_интервалы(clock: Clock) -> None:
    """Тест утверждает о воздействии по часам платы, а не по своему заказу."""
    board, api = waving(clock)

    board.wave(board.line(channel=1, edges=[500, 300, 500]))

    assert board.delivered(channel=1) == [500, 300, 500]
    moments = board.moments(channel=1)
    assert len(moments) == 4, f'моментов {len(moments)}, участков 3: плюс отпускание'
    assert moments == sorted(moments)
    assert api.waves[0][0]['pin'] == 3


def test_расписка_без_нужного_вывода_не_проходит_молча(clock: Clock) -> None:
    """
    Пустая расписка - это «плата не подала», а не «подала как заказано».
    Молчаливое согласие здесь вернуло бы тесты к вере в заказ.
    """
    board, api = waving(clock)
    api._marks = {}

    with pytest.raises(AssertionError, match='нет вывода 2'):
        board.delivered(channel=0)


def test_нажатие_видно_в_логе(clock: Clock, monkeypatch: pytest.MonkeyPatch) -> None:
    """
    Без метки о нажатии молчание устройства не с чем сопоставить: в логе прогона
    стоит только приговор «Ватериус не проснулся», а когда его будили - нет.
    """
    сказано: list[str] = []
    monkeypatch.setattr(dut_mod.logger, 'info', lambda text: сказано.append(text))
    board, _ = make(clock)

    board.press_button()
    board.hold_button()

    assert f'{BUTTON_SHORT_MS} мс' in сказано[0], сказано
    assert f'{BUTTON_SETUP_MS} мс' in сказано[1], сказано
    assert 'удержание' in сказано[1], сказано


def test_импульсы_подъёма_уходят_пачкой_и_дают_расписку(clock: Clock) -> None:
    """
    Импульс электронного входа - подъём линии, и форму ему обязана отмерять
    плата: тридцать миллисекунд, набранные двумя запросами по радио, - это
    заказ плюс дорога, а осечка связи добавляет туда же паузу повтора.
    """
    board, api = waving(clock)

    подано = board.pulse_high(channel=1, count=3, width_ms=30, gap=1.2)

    assert len(api.waves) == 1, f'пачек {len(api.waves)}, а нужна одна'
    заказ = api.waves[0][0]
    assert заказ['value'] == dut_mod.HIGH, 'импульс электронного входа - подъём'
    assert заказ['edges'] == [30, 1200, 30, 1200, 30, 1200], (
        f'пачка обязана кончаться паузой, чтобы линию отпустили с уровня '
        f'покоя: {заказ["edges"]}')
    assert подано == 3, f'по расписке платы импульсов {подано}, а заказано 3'


def test_после_пачки_подъёма_линия_возвращается_в_покой(clock: Clock) -> None:
    """
    Последний момент пачки - отпускание линии, а отпущенный высокоомный вход
    ловит эфир: импульсы возьмутся из ниоткуда. Покой возвращает стенд сразу
    после пачки.
    """
    board, api = waving(clock)

    board.pulse_high(channel=1, count=1, width_ms=30, gap=1.2)

    assert len(api.lows) == 1, (
        f'линию не вернули в покой после пачки: прижатий {len(api.lows)}')


def test_серия_с_нажатием_режется_на_пачки(clock: Clock) -> None:
    """
    Пачка платы вмещает восемь импульсов, а серии поверх сеанса нужно больше.
    Нажатие едет в первой пачке, иначе его момент не сравнить с импульсами.
    """
    board, api = waving(clock)

    серия = board.series({0: (500, 0), 1: (1, 100)}, count=10, step_ms=1800,
                         press_after=2)

    assert [len(wave) for wave in api.waves] == [3, 2], (
        f'линий по пачкам {[len(w) for w in api.waves]}: кнопка - только в первой')
    кнопка = [line for line in api.waves[0] if line['pin'] == 1]
    assert кнопка and кнопка[0]['edges'] == [BUTTON_SHORT_MS]
    assert [len(серия.closures[ch]) for ch in (0, 1)] == [10, 10]
    assert серия.press[1] - серия.press[0] == BUTTON_SHORT_MS


def test_серия_держит_паузу_на_стыке_пачек(clock: Clock) -> None:
    """
    Между пачками стоит дорога по радио, и вплотную подданные замыкания attiny
    честно сочла бы одним. Пауза на стыке не короче паузы внутри пачки.
    """
    board, _ = waving(clock)

    серия = board.series({0: (500, 0)}, count=12, step_ms=1800, press_after=0)

    замыкания = серия.closures[0]
    паузы = [после[0] - до[1] for до, после in zip(замыкания, замыкания[1:], strict=False)]
    assert min(паузы) >= 1300, f'паузы между замыканиями: {паузы}'


def test_нажатие_ложится_между_импульсами(clock: Clock) -> None:
    """Нажатие после второго импульса, а не поверх какого-нибудь из них."""
    board, _ = waving(clock)

    серия = board.series({0: (500, 0)}, count=4, step_ms=1800, press_after=2)

    второй, третий = серия.closures[0][1], серия.closures[0][2]
    assert второй[1] < серия.press[0] and серия.press[1] < третий[0], (
        f'нажатие {серия.press}, импульсы {серия.closures[0]}')


def test_серия_перед_нажатием_выжидает_сеанс(clock: Clock) -> None:
    """Выдержку после прошлого сеанса нажатие внутри серии получает так же."""
    выдержки: list[int] = []
    api = WavingApi(clock)
    board = dut_mod.Dut(api, button_pin=1, ch0_pin=2, ch1_pin=3, reset_pin=4,
                        settle=выдержки.append)
    board._led_ok = False

    board.series({1: (1, 0)}, count=10, step_ms=1800, press_after=2)

    assert выдержки == [BUTTON_SHORT_MS]


def test_серия_со_слитыми_замыканиями_не_заказывается(clock: Clock) -> None:
    board, api = waving(clock)

    with pytest.raises(ValueError, match='сольются'):
        board.series({0: (500, 0)}, count=4, step_ms=1000, press_after=1)
    assert not api.waves


def test_после_нажатия_импульс_ждёт_всю_паузу(clock: Clock) -> None:
    """
    Окно сеанса открывается через WAKE_MARGIN_MS после нажатия. Импульс,
    вставший к нажатию ближе, в сеанс не засчитывается, и пятисекундный сеанс
    вмещал бы на один импульс меньше.
    """
    board, _ = waving(clock)

    серия = board.series({0: (500, 0), 1: (1, 100)}, count=8, step_ms=1800,
                         press_after=2)

    следующий = серия.closures[0][2][0]
    assert следующий - серия.press[0] >= WAKE_MARGIN_MS, (
        f'нажатие {серия.press}, следующее замыкание в {следующий}')


def test_в_сеанс_идут_только_целые_импульсы_после_пробуждения() -> None:
    """Импульс на краю окна мог прийтись на сон, и счёт посреди сеанса он не доказывает."""
    серия = Series(closures={0: [(500, 1000), (1300, 1800), (3400, 3900), (5200, 5700)]},
                   press=(1000, 1500))

    в_сеансе = серия.awake(0, length_ms=4500)

    assert в_сеансе == [(3400, 3900)], в_сеансе


def test_механика_ложится_в_короткий_сеанс(clock: Clock) -> None:
    """
    Сеанс по кнопке на стенде - около 5 с, и его длину двигает ответ облака.
    Замыкания 350 мс через секунду обязаны лечь по два и в сеанс 3,6 с.
    """
    board, _ = waving(clock)

    серия = board.series({0: (350, 0)}, count=10, step_ms=1350, press_after=2,
                         min_gap_ms=1000)

    assert len(серия.awake(0, length_ms=3600)) == 2, серия
