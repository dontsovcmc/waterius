"""
Клиент программатора без железа: порт подменён, ответы — дословно как у прошивки 0.7.0
(waterius_flasher, core/port.cpp).
"""

from __future__ import annotations

from collections.abc import Callable

import pytest
import serial

from .. import programmer as prog_mod
from ..programmer import (
    LOG_CHUNK,
    LineBusy,
    NoBoard,
    Programmer,
    ProgrammerGone,
    ProgrammerTooOld,
    WaveRefused,
)


class FakeLink:
    """Порт, который отвечает на каждую команду функцией теста."""

    def __init__(self, answer: Callable[[str], bytes]) -> None:
        self.answer = answer
        self.sent: list[str] = []
        self.dtr = False
        self.dtr_changes = 0
        self._out = b''

    def __setattr__(self, name: str, value: object) -> None:
        if name == 'dtr' and 'dtr' in self.__dict__ and value != self.__dict__['dtr']:
            self.__dict__['dtr_changes'] += 1
        super().__setattr__(name, value)

    def write(self, data: bytes) -> int:
        line = data.decode().rstrip('\n')
        self.sent.append(line)
        self._out += self.answer(line)
        return len(data)

    def readline(self) -> bytes:
        at = self._out.find(b'\n')
        if at < 0:                       # молчание: столько отдаёт pyserial по таймауту
            out, self._out = self._out, b''
            return out
        out, self._out = self._out[:at + 1], self._out[at + 1:]
        return out

    def read(self, size: int) -> bytes:
        out, self._out = self._out[:size], self._out[size:]
        return out

    def reset_input_buffer(self) -> None:
        self._out = b''

    def close(self) -> None:
        pass


def board(answers: dict[str, str]) -> tuple[Programmer, FakeLink]:
    link = FakeLink(lambda line: (answers[line] + '\n').encode() if line in answers else b'')
    return Programmer(link, '/dev/fake'), link


def test_пачка_собирается_одной_командой() -> None:
    prog, link = board({'#wave ch1 0 0 500,1500,500 button 0 4001 500': '# wave: 4501'})

    length = prog.wave([{'line': 'ch1', 'value': 0, 'at_ms': 0, 'edges': [500, 1500, 500]},
                        {'line': 'button', 'at_ms': 4001, 'edges': [500]}])

    assert length == 4501
    assert link.sent == ['#wave ch1 0 0 500,1500,500 button 0 4001 500']


def test_отказ_в_пачке_несёт_текст_программатора() -> None:
    prog, _ = board({'#wave ch1 0 0 500': '# wave: линия занята: ch1'})

    with pytest.raises(WaveRefused, match='линия занята: ch1'):
        prog.wave([{'line': 'ch1', 'edges': [500]}])


def test_расписка_разбирается_из_json() -> None:
    stat = ('{"uptime_ms":9000,"start_ms":1000,"busy":false,"lines":[{"line":"ch1",'
            '"value":0,"at_ms":0,"asked":[500],"edges_ms":[1000,1500]}]}')
    prog, _ = board({'#wave stat': f'# wave: {stat}'})

    assert prog.wave_stat()['lines'][0]['edges_ms'] == [1000, 1500]


def test_снять_удержание_с_линии_пачки_нельзя() -> None:
    prog, _ = board({'#release ch0': '# release: линия занята: ch0', '#hold ch0': '# hold: ch0'})

    prog.hold('ch0')
    with pytest.raises(LineBusy):
        prog.release('ch0')


def test_лог_выбирается_до_дна_и_помнит_потерю() -> None:
    chunks = [b'a' * LOG_CHUNK, b'b' * 10]
    flags = ['1', '0']

    def answer(line: str) -> bytes:
        assert line == '#log'
        data = chunks.pop(0)
        return f'# log: {len(data)} {flags.pop(0)}\n'.encode() + data

    prog = Programmer(FakeLink(answer), '/dev/fake')

    text, lost = prog.log_read()

    assert text == 'a' * LOG_CHUNK + 'b' * 10
    assert lost


def test_буква_на_стыке_кусков_не_бьётся() -> None:
    """Кусок `#log` режет поток по байтам, а не по символам."""
    буква = 'я'.encode()
    chunks = [b'x' * (LOG_CHUNK - 1) + буква[:1], буква[1:] + b'\n']

    def answer(line: str) -> bytes:
        data = chunks.pop(0)
        return f'# log: {len(data)} 0\n'.encode() + data

    prog = Programmer(FakeLink(answer), '/dev/fake')

    text, _ = prog.log_read()

    assert text.endswith('я\n')


def test_молчание_программатора_останавливает_прогон() -> None:
    prog, _ = board({})

    with pytest.raises(ProgrammerGone, match='не ответил на «#board»'):
        prog.board()


def test_обрыв_порта_останавливает_прогон() -> None:
    def answer(line: str) -> bytes:
        raise serial.SerialException('device disconnected')

    prog = Programmer(FakeLink(answer), '/dev/fake')

    with pytest.raises(ProgrammerGone, match='пропал'):
        prog.log_read()


def test_из_моста_выходит_сменой_dtr() -> None:
    state = {'bridge': True}

    def answer(line: str) -> bytes:
        if state['bridge']:
            return b''
        return b'# waterius_flasher 0.7.0 uptime=5\n'

    link = FakeLink(answer)
    prog = Programmer(link, '/dev/fake')
    original = FakeLink.reset_input_buffer

    def leave_bridge(self: FakeLink) -> None:
        state['bridge'] = self.dtr_changes == 0
        original(self)

    link.reset_input_buffer = leave_bridge.__get__(link)   # type: ignore[method-assign]

    prog.wake()

    assert link.dtr_changes == 2 and link.dtr is False
    assert prog.version == (0, 7, 0)


def test_досмотр_старая_прошивка() -> None:
    prog, _ = board({'#info': '# waterius_flasher 0.6.3 uptime=12'})

    with pytest.raises(ProgrammerTooOld, match='0.6.3.*0.7.0'):
        prog_mod.check(prog)


def test_досмотр_платы_нет() -> None:
    prog, _ = board({'#info': '# waterius_flasher 0.7.0 uptime=12', '#board': '# board: нет'})

    with pytest.raises(NoBoard, match='платы на прищепке нет'):
        prog_mod.check(prog)


def test_досмотр_всё_на_месте() -> None:
    prog, _ = board({'#info': '# waterius_flasher 0.7.0 uptime=12', '#board': '# board: есть'})

    assert 'прошивка 0.7.0' in prog_mod.check(prog)


def test_подъём_лога_начинает_с_отпущенных_линий() -> None:
    prog, link = board({'#lines off': '# lines: отпущены', '#esp log 115200': '# esp: лог 115200'})

    prog.log_begin()

    assert link.sent == ['#lines off', '#esp log 115200']
