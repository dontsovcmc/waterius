"""
Разбор скана эфира проверяется без железа.

Образец - вывод `system_profiler SPAirPortDataType` того самого Мака, на
котором стоит стенд: своя сеть, соседи и точка стенда. Смысл теста не «regexp
написан», а «полосу и канал мы читаем у того, кто их слышит»: у роутера стенда
команды на полосу нет, и это единственный источник.
"""

from __future__ import annotations

import pytest

from .. import air

SAMPLE = """
Wi-Fi:

      Software Versions:
          CoreWLAN: 17.0
      Interfaces:
        en0:
          Status: Connected
          Current Network Information:
            dav:
              PHY Mode: 802.11n
              Channel: 4 (2GHz, 20MHz)
              Network Type: Infrastructure
              Signal / Noise: -67 dBm / -97 dBm
          Other Local Wi-Fi Networks:
            GAL:
              PHY Mode: 802.11n
              Channel: 6 (2GHz, 20MHz)
            Silk-685058:
              PHY Mode: 802.11n
              Channel: 11 (2GHz, 20MHz)
            waterius_stand:
              PHY Mode: 802.11n
              Channel: 6 (2GHz, 40MHz)
              Signal / Noise: -41 dBm / -97 dBm
          awdl0:
"""


@pytest.fixture
def scanned(monkeypatch: pytest.MonkeyPatch) -> list[air.Network]:
    class Done:
        stdout = SAMPLE

    monkeypatch.setattr(air.subprocess, 'run', lambda *a, **k: Done())
    return air.scan()


def test_скан_называет_канал_полосу_и_сигнал(scanned: list[air.Network]) -> None:
    ours = next(n for n in scanned if n.ssid == 'waterius_stand')
    assert (ours.channel, ours.width, ours.rssi) == (6, 40, -41)
    # Своя сеть лежит в другом разделе вывода, но нужна так же: в ней сидят
    # Мак с приёмником и METF
    assert next(n for n in scanned if n.ssid == 'dav').channel == 4
    # Заголовки разделов - не сети
    assert not [n for n in scanned if n.ssid.endswith(('Networks', 'Information'))]


def test_широкая_полоса_названа_бедой(scanned: list[air.Network],
                                       monkeypatch: pytest.MonkeyPatch,
                                       caplog: pytest.LogCaptureFixture) -> None:
    said: list[str] = []
    monkeypatch.setattr(air, 'scan', lambda want='': scanned)
    monkeypatch.setattr(air.logger, 'warning', lambda text: said.append(text))
    monkeypatch.setattr(air.logger, 'info', lambda text: said.append(text))

    air.check('waterius_stand', 6, 20)
    assert any('40 МГц' in text and '20' in text for text in said), said
    # Соседи в полосе - тоже улика: канал 4 и 6 перекрываются
    assert any('dav' in text and 'GAL' in text for text in said), said


def test_нужная_полоса_молчит(scanned: list[air.Network],
                              monkeypatch: pytest.MonkeyPatch) -> None:
    said: list[str] = []
    monkeypatch.setattr(air, 'scan', lambda want='': scanned)
    monkeypatch.setattr(air.logger, 'warning', lambda text: said.append(text))
    monkeypatch.setattr(air.logger, 'info', lambda text: None)

    air.check('waterius_stand', 6, 40)
    assert not [text for text in said if 'полосой' in text], said


def test_скан_без_сканера_не_роняет(monkeypatch: pytest.MonkeyPatch) -> None:
    def нет(*a: object, **k: object) -> None:
        raise OSError('нет такой команды')

    monkeypatch.setattr(air.subprocess, 'run', нет)
    assert air.scan() == []
