"""
Лог прогона: исход теста не должен отрываться от его имени.

В файле прогона 2026-09-28 09-56 между именем теста и словом PASSED легло
1325 строк его же лога, и в файле осталась сирота «PASSED     1h 32m» - чей
это исход, по файлу узнать было нельзя.
"""
from ..conftest import _Lines


class Файл:
    """Приёмник вместо файла: копит написанное."""

    def __init__(self) -> None:
        self.text = ''

    def write(self, data: str) -> int:
        self.text += data
        return len(data)

    def flush(self) -> None:
        pass


ИМЯ = 'Utils/hil/test_alarms.py::test_E3n_single_pulse_is_not_a_leak '


def test_исход_печатается_вместе_с_именем_теста() -> None:
    файл = Файл()
    строки = _Lines(файл)

    строки.raw(ИМЯ)                       # pytest: имя теста, без перевода
    строки.write('11:29:24 | INFO | ping\n')   # логгер посреди строки
    строки.raw('PASSED     1h 32m\n')     # pytest: исход, через час

    assert файл.text.endswith(ИМЯ + 'PASSED     1h 32m\n')
    assert '\nPASSED' not in файл.text    # сироты быть не должно


def test_запись_логгера_не_приклеивается_к_строке_теста() -> None:
    файл = Файл()
    строки = _Lines(файл)

    строки.raw(ИМЯ)
    строки.write('11:29:24 | INFO | ping\n')

    assert файл.text == ИМЯ + '\n11:29:24 | INFO | ping\n'


def test_неразорванная_строка_не_повторяется() -> None:
    """Селф-тесты идут без записей логгера между частями строки."""
    файл = Файл()
    строки = _Lines(файл)

    строки.raw(ИМЯ)
    строки.raw('PASSED 0.4s\n')

    assert файл.text == ИМЯ + 'PASSED 0.4s\n'


def test_повтор_только_один_раз() -> None:
    """Имя печатается заново один раз, а не перед каждой частью исхода."""
    файл = Файл()
    строки = _Lines(файл)

    строки.raw(ИМЯ)
    строки.write('11:29:24 | INFO | ping\n')
    строки.raw('PASSED')
    строки.raw('     1h 32m\n')

    assert файл.text.count('test_E3n') == 2
