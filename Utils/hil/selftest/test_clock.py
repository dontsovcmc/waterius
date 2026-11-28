"""
Сервер времени роутера без железа: консоль подменена, ответы - как у `2.5.0-waterius.1`
(`components/ntp_server/cmd_ntp.c` форка).
"""

from __future__ import annotations

import pytest

from ..clock import RouterClock
from ..router import NatRouter, RouterError

STAT = ('{"running":true,"dropping":false,"epoch":1767225600,"requests":3,"replies":3,'
        '"dropped":0,"ignored":0,"last_epoch":1767225600,"last_client":"192.168.4.100"}')


class Console:
    def __init__(self, answers: dict[str, str]) -> None:
        self.answers = answers
        self.sent: list[str] = []

    def run(self, line: str, timeout: float) -> str:
        self.sent.append(line)
        return self.answers.get(line, 'Unrecognized command\n')

    def close(self) -> None:
        pass


def clock(answers: dict[str, str]) -> tuple[RouterClock, Console]:
    console = Console(answers)
    return RouterClock(NatRouter(console)), console


def test_команды_уходят_в_консоль() -> None:
    board, console = clock({'ntp start 1767225600': 'ok\n', 'ntp drop 1': 'ok\n',
                            'ntp time 1800000000': 'ok\n', 'ntp stop': 'ok\n'})

    board.start(1767225600)
    board.drop(True)
    board.set_time(1800000000)
    board.stop()

    assert console.sent == ['ntp start 1767225600', 'ntp drop 1', 'ntp time 1800000000',
                            'ntp stop']


def test_счётчик_запросов_из_stat() -> None:
    board, _ = clock({'ntp stat': STAT + '\n'})

    assert board.requests_seen == 3
    assert board.stat()['running'] is True


def test_отказ_роутера_не_проходит_молча() -> None:
    board, _ = clock({'ntp time 1': 'ntp: not running\n'})

    with pytest.raises(RouterError, match='not running'):
        board.set_time(1)


def test_роутер_без_сервера_времени() -> None:
    """Прошивка без `-waterius` команды не знает - стенд говорит это до первого теста."""
    board, _ = clock({})

    assert board.available() is False
