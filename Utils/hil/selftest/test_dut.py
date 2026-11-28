"""
Паузы между импульсами не оставляют лог устройства без присмотра.

Железо не нужно: программатор заменяет заглушка, а часами управляет тест. Проверяется
то, из-за чего падал E2: пока пауза была одним `sleep`, устройство успевало
проснуться по расписанию, напечатать сотни строк, и кольцо лога переполнялось.
Тест падал не на своей проверке, а на «лог неполон, утверждать по нему нечего».
"""

from __future__ import annotations

import pytest

from .. import dut as dut_mod
from ..dut import BUTTON_SETUP_MS, BUTTON_SHORT_MS
from ..programmer import LineBusy


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
    """
    Программатор: запоминает заказ и отдаёт по нему расписку так, как отдал бы
    настоящий - моментами фронтов по своим часам. Время не двигает: ждёт стенд.
    """

    def __init__(self, clock: Clock) -> None:
        self.clock = clock
        self.lows: list[float] = []       # когда прижималась линия
        self.waves: list[list[dict]] = []
        self.held: set[str] = set()
        self.released: list[str] = []
        self.busy: dict[str, int] = {}    # линия -> сколько раз ответить «занята»
        self._marks: dict[str, list[int]] = {}

    def wave(self, lines: list[dict]) -> int:
        self.waves.append(lines)
        start = int(self.clock.now * 1000)
        longest = 0
        for line in lines:
            at = line.get('at_ms', 0)
            self.lows.append(self.clock.now + at / 1000)
            marks = [start + at]
            for edge in line['edges']:
                marks.append(marks[-1] + edge)
            self._marks[line['line']] = marks
            longest = max(longest, marks[-1] - start)
        return longest

    def wave_stat(self) -> dict:
        return {'lines': [{'line': line, 'edges_ms': marks}
                          for line, marks in self._marks.items()]}

    def hold(self, line: str) -> None:
        self.held.add(line)

    def release(self, line: str) -> None:
        if self.busy.get(line):
            self.busy[line] -= 1
            self.clock.sleep(0.1)
            raise LineBusy(f'release {line}')
        self.held.discard(line)
        self.released.append(line)


@pytest.fixture
def clock(monkeypatch: pytest.MonkeyPatch) -> Clock:
    fake = Clock()
    monkeypatch.setattr(dut_mod, 'time', fake)
    return fake


def make(clock: Clock, idle=None, settle=None) -> tuple[dut_mod.Dut, FakeApi]:
    api = FakeApi(clock)
    return dut_mod.Dut(api, idle=idle, settle=settle), api


def test_пауза_вычитывает_лог(clock: Clock) -> None:
    """Минута между импульсами - это десятки обращений к плате, а не одно."""
    reads: list[float] = []
    board, _ = make(clock, idle=lambda: reads.append(clock.now))

    board.pulses(channel=0, count=2, gap=60.0, width_ms=500)

    assert len(reads) >= 50, (
        f'за минуту паузы лог вычитан {len(reads)} раз: кольцо программатора '
        'переполнится плановым пробуждением')


def test_ритм_импульсов_не_плывёт(clock: Clock) -> None:
    """
    Вычитывание не имеет права сдвинуть импульс: ритм сравнивается с порогом
    тревоги. Дедлайн абсолютный, поэтому съеденное время не накапливается.
    """
    board, api = make(clock, idle=lambda: clock.sleep(0.015))  # опрос программатора

    board.pulses(channel=0, count=4, gap=60.0, width_ms=500)

    starts = api.lows
    assert len(starts) == 4
    for i, moment in enumerate(starts):
        drift = abs(moment - (starts[0] + i * 60.0))
        assert drift < 0.5, f'импульс {i} уехал на {drift:.3f} с'


def test_догоняя_расписание_серия_не_сливает_замыкания(clock: Clock) -> None:
    """
    Задержка посреди серии (долгий опрос лога): следующие сроки уже в прошлом, но
    замыкание всё равно не встаёт вплотную к предыдущему - attiny слил бы их
    в один импульс.
    """
    board, api = make(clock)
    отлучка = {'после': 2, 'на': 4.0}
    подать = api.wave

    def с_отлучкой(lines: list[dict]) -> int:
        if len(api.lows) == отлучка['после']:
            clock.sleep(отлучка['на'])
        return подать(lines)

    api.wave = с_отлучкой                          # type: ignore[method-assign]

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


def waving(clock: Clock) -> tuple[dut_mod.Dut, FakeApi]:
    return make(clock)


def test_серия_уходит_одной_пачкой(clock: Clock) -> None:
    """
    Интервалы внутри серии отмеряет программатор: стенд отмерял бы их со своей
    дорогой до порта.
    """
    board, api = waving(clock)

    подано = board.pulse(channel=0, count=3, width_ms=500, gap=1.2)

    assert len(api.waves) == 1, f'пачек {len(api.waves)}, а нужна одна'
    assert api.waves[0][0]['edges'] == [500, 1200, 500, 1200, 500]
    assert подано == 3, f'по расписке программатора замыканий {подано}, а заказано 3'


def test_длинная_пауза_режет_серию_на_пачки_по_одному(clock: Clock) -> None:
    """
    Пачка не длиннее полминуты, и это не произвол: в длинных паузах
    стенд вычитывает лог устройства, иначе кольцо программатора переполняется. Но
    замыкание всё равно заказывается пачкой - иначе у стенда нет расписки, и
    «до входа дошло 9 импульсов из 10» не отличить от «стенд не довёл».
    """
    board, api = waving(clock)

    подано = board.pulse(channel=0, count=2, width_ms=500, gap=60.0)

    assert [line[0]['edges'] for line in api.waves] == [[500], [500]], (
        f'минутную паузу отдали плате: {api.waves}')
    assert подано == 2


def test_расписка_даёт_фактические_интервалы(clock: Clock) -> None:
    """Тест утверждает о воздействии по часам программатора, а не по своему заказу."""
    board, api = waving(clock)

    board.wave(board.line(channel=1, edges=[500, 300, 500]))

    assert board.delivered(channel=1) == [500, 300, 500]
    moments = board.moments(channel=1)
    assert len(moments) == 4, f'моментов {len(moments)}, участков 3: плюс отпускание'
    assert moments == sorted(moments)
    assert api.waves[0][0]['line'] == 'ch1'


def test_расписка_без_нужного_вывода_не_проходит_молча(clock: Clock) -> None:
    """
    Пустая расписка - это «программатор не подал», а не «подала как заказано».
    Молчаливое согласие здесь вернуло бы тесты к вере в заказ.
    """
    board, api = waving(clock)
    api._marks = {}

    with pytest.raises(AssertionError, match='нет линии ch0'):
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


def test_серия_с_нажатием_режется_на_пачки(clock: Clock) -> None:
    """
    Пачка вмещает шестнадцать импульсов, а серии поверх сеанса бывает нужно больше.
    Нажатие едет в первой пачке, иначе его момент не сравнить с импульсами.
    """
    board, api = waving(clock)

    серия = board.series({0: (500, 0), 1: (1, 100)}, count=20, step_ms=1800,
                         press_after=2)

    assert [len(wave) for wave in api.waves] == [3, 2], (
        f'линий по пачкам {[len(w) for w in api.waves]}: кнопка - только в первой')
    кнопка = [line for line in api.waves[0] if line['line'] == 'button']
    assert кнопка and кнопка[0]['edges'] == [BUTTON_SHORT_MS]
    assert [len(серия.closures[ch]) for ch in (0, 1)] == [20, 20]
    assert серия.press[1] - серия.press[0] == BUTTON_SHORT_MS


def test_серия_держит_паузу_на_стыке_пачек(clock: Clock) -> None:
    """
    Между пачками стоит дорога до порта, и вплотную подданные замыкания attiny
    честно сочла бы одним. Пауза на стыке не короче паузы внутри пачки.
    """
    board, _ = waving(clock)

    серия = board.series({0: (500, 0)}, count=24, step_ms=1800, press_after=0)

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
    board, _ = make(clock, settle=выдержки.append)

    board.series({1: (1, 0)}, count=10, step_ms=1800, press_after=2)

    assert выдержки == [BUTTON_SHORT_MS]


def test_серия_со_слитыми_замыканиями_не_заказывается(clock: Clock) -> None:
    board, api = waving(clock)

    with pytest.raises(ValueError, match='сольются'):
        board.series({0: (500, 0)}, count=4, step_ms=1000, press_after=1)
    assert not api.waves



def test_уборка_ждёт_конца_пачки_и_снимает_удержания(clock: Clock) -> None:
    """
    Уборка после теста отпускает все линии, не закрывая лог: удержание снимается,
    а линию с идущей пачкой отпустит конец пачки - его уборка и ждёт.
    """
    board, api = make(clock)
    board.wet(channel=1, closed=True)
    api.busy['ch0'] = 3

    board.init()

    assert not api.held
    assert sorted(api.released) == ['button', 'ch0', 'ch1', 'rst']


def test_уборка_не_ждёт_дольше_самой_длинной_пачки(clock: Clock) -> None:
    board, api = make(clock)
    api.busy['ch0'] = 10_000

    with pytest.raises(LineBusy):
        board.init()
    assert clock.now - 1000.0 <= dut_mod.RELEASE_WAIT_S + 1
