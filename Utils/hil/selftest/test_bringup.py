"""
Подъём стенда и выбор канала проверяются без железа.

Оба правила стоят дорого именно в прогоне: `bring_up` решает, уносит ли одна
осечка связи весь набор, а запасной канал - проверяет ли тест смены канала
прошивку или удачу.
"""

from __future__ import annotations

from dataclasses import dataclass

import pytest

from ..conftest import bring_up
from ..test_wifi import other_channel


def падать(сколько: int, текст: str):
    """Шаг подъёма, который падает первые `сколько` раз."""
    осталось = {'n': сколько}

    def шаг() -> str:
        if осталось['n']:
            осталось['n'] -= 1
            raise AssertionError(текст)
        return 'поднялся'

    return шаг


@pytest.mark.parametrize('текст', [
    'METF потеряла кольцо: чтение лога оборвалось',
    'стенд ослеп: за 20.0 с METF ни разу не отдала лог',
    'приёмник получил 3 нечитаемое тело посылки и ни одного целого: передача '
    'оборвалась на середине, и о состоянии устройства этот сеанс не говорит ничего',
])
def test_беда_стенда_повторяется(текст: str) -> None:
    assert bring_up(падать(1, текст), 'опрос устройства') == 'поднялся'


def test_отказ_по_делу_летит_сразу() -> None:
    # Устройство молчит при живой связи - это про прошивку, и повтор только
    # спрячет дефект за второй попыткой
    with pytest.raises(AssertionError, match='не проснулся'):
        bring_up(падать(1, 'Ватериус не проснулся: за 2 с на UART ни одной строки'),
                 'опрос устройства')


def test_беда_дважды_подряд_не_прощается() -> None:
    # Повтор один: если стенд слеп и во второй раз, прогон бессмыслен
    with pytest.raises(AssertionError, match='ослеп'):
        bring_up(падать(2, 'стенд ослеп: METF ни разу не отдала лог'), 'сеть стенда')


@dataclass(frozen=True)
class Настройки:
    ap_channel_other: int


def test_запасной_канал_берётся_из_настроек() -> None:
    assert other_channel(Настройки(ap_channel_other=1), 11) == 1


def test_запасной_канал_совпал_с_рабочим() -> None:
    # Тест смены канала на таком стенде ничего не проверит, и молчать об этом
    # нельзя: он станет зелёным, не сменив канала
    with pytest.raises(AssertionError, match='совпал с рабочим'):
        other_channel(Настройки(ap_channel_other=11), 11)


class ПоддельныйСтенд:
    """Стенд, у которого спрашивают только то, что нужно уликам про эфир."""

    class api:                       # noqa: N801 - подделка, а не класс стенда
        @staticmethod
        def wifi() -> dict[str, object]:
            return {'rssi': -71, 'ssid': 'dav', 'channel': 4}

    last_payload = {'rssi': -83, 'channel': 11, 'wifi_connect_errors': 14}


def test_улики_про_эфир_называют_обе_платы(monkeypatch: pytest.MonkeyPatch) -> None:
    from .. import conftest as ветка

    сказано: list[str] = []
    monkeypatch.setattr(ветка, '_stand', ПоддельныйСтенд())
    monkeypatch.setattr(ветка.logger, 'info', lambda text: сказано.append(text))

    ветка._log_air('test_sending.py')
    assert len(сказано) == 1
    assert '-71' in сказано[0] and '-83' in сказано[0], сказано


def test_молчащая_METF_не_роняет_улики(monkeypatch: pytest.MonkeyPatch) -> None:
    from .. import conftest as ветка

    class Немая(ПоддельныйСтенд):
        class api:                   # noqa: N801
            @staticmethod
            def wifi() -> dict[str, object]:
                raise TimeoutError('нет ответа')

    сказано: list[str] = []
    monkeypatch.setattr(ветка, '_stand', Немая())
    monkeypatch.setattr(ветка.logger, 'info', lambda text: сказано.append(text))

    ветка._log_air('test_sending.py')
    assert 'TimeoutError' in сказано[0] and '-83' in сказано[0], сказано
