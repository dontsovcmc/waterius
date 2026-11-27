"""
Обнаружение простоя хоста.

Прогон 2026-09-27 дал девять ложных падений из десяти: Мак засыпал на 15-17
минут, настенные часы за это время уходили вперёд монотонных, и стенд объявлял
«сеанс не доиграл за 125 с» - при том что pytest намерил на тест 2,56 с.
"""
import pytest

from .. import hostclock
from ..hostclock import HostAsleep, HostClock


class Clock:
    """Двое часов под управлением теста: сон хоста двигает только настенные."""

    def __init__(self) -> None:
        self.mono = 1000.0
        self.wall = 500000.0

    def monotonic(self) -> float:
        return self.mono

    def time(self) -> float:
        return self.wall

    def работа(self, seconds: float) -> None:
        self.mono += seconds
        self.wall += seconds

    def сон_хоста(self, seconds: float) -> None:
        """Во сне монотонные часы стоят, а настенные идут."""
        self.wall += seconds


@pytest.fixture
def clock(monkeypatch: pytest.MonkeyPatch) -> Clock:
    fake = Clock()
    monkeypatch.setattr(hostclock, 'time', fake)
    return fake


def test_работа_без_простоя_не_замечается(clock: Clock) -> None:
    часы = HostClock()
    clock.работа(300.0)

    assert часы.tick() == 0.0
    assert часы.slept == 0.0
    assert часы.note() == ''


def test_сон_хоста_виден_и_копится(clock: Clock) -> None:
    часы = HostClock()
    clock.работа(2.0)
    clock.сон_хоста(1039.0)

    assert часы.tick() == pytest.approx(1039.0)
    clock.работа(5.0)
    clock.сон_хоста(919.0)
    assert часы.tick() == pytest.approx(919.0)

    assert часы.slept == pytest.approx(1958.0)
    assert 'простоял 1958 с' in часы.note()
    assert 'последний провал 919 с' in часы.note()


def test_мелкое_расхождение_не_простой(clock: Clock) -> None:
    """Шаг ntpd на секунду - не сон: иначе прогон падал бы от синхронизации часов."""
    часы = HostClock()
    clock.работа(10.0)
    clock.сон_хоста(hostclock.HOST_JUMP_S - 1.0)

    assert часы.tick() == 0.0
    assert часы.slept == 0.0


def test_простой_останавливает_ожидание(clock: Clock) -> None:
    """
    `check` поднимает не AssertionError: это не приговор устройству, и разбирать
    его вместе с отказами прошивки нельзя.
    """
    часы = HostClock()
    clock.сон_хоста(954.0)

    with pytest.raises(HostAsleep) as err:
        часы.check('ожидание сеанса')

    assert 'не работал 954 с' in str(err.value)
    assert 'ожидание сеанса' in str(err.value)
    assert not isinstance(err.value, AssertionError)
