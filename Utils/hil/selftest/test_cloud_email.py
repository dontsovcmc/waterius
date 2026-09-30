"""
Облако стенда и почта учётной записи в требованиях к устройству - без железа.

Адрес облака стенд прописывает устройству сам: прогон не ходит в интернет, а
сверяет то, за что отвечает прошивка. Почта - из stand.ini, и пустую стенд не
трогает.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from .. import config as stand_config
from ..stand import Stand

СТЕНД = """\
[metf]
host = 192.168.100.21

[router]
host = 192.168.100.20

[dut]
mac = 24:4C:AB:68:2E:BA
email = test@waterius.ru

[receiver]
host = 192.168.100.18
port = 8010

[broker]
host = 192.168.100.18
port = 1883
"""


@pytest.fixture
def файл(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    monkeypatch.setattr(stand_config, 'own_ip', lambda towards: '192.168.100.18')
    path = tmp_path / 'stand.ini'
    path.write_text(СТЕНД, encoding='utf-8')
    return path


ОБЛАКО = 'http://192.168.100.18:8020/cloud'


class Стенд:
    """Стенд, у которого спрашивают только требования к устройству."""

    mqtt = None
    cloud_url = ОБЛАКО

    def __init__(self, cfg: Any) -> None:
        self.cfg = cfg


class Настройки:
    """Часть stand.ini, которую читает Stand.requirements."""

    metf_host = '192.168.100.21'
    http_url = 'http://192.168.100.18:8010/'

    def __init__(self, email: str) -> None:
        self.dut_email = email


def test_почта_читается_из_файла(файл: Path) -> None:
    assert stand_config.load(файл).dut_email == 'test@waterius.ru'


def test_без_строки_почты_остаётся_пусто(файл: Path) -> None:
    файл.write_text(СТЕНД.replace('email = test@waterius.ru\n', ''),
                    encoding='utf-8')
    assert stand_config.load(файл).dut_email == ''


def test_почта_попадает_в_общие_требования() -> None:
    """Иначе её вернул бы только заводской сброс, а пустую - никто."""
    want = Stand.requirements(Стенд(Настройки('test@waterius.ru')))
    assert want['waterius_email'] == 'test@waterius.ru'
    # Выключенный получатель почту не принимает, а параметры применяются по
    # порядку ключей ответа
    имена = list(want)
    assert имена.index('waterius_on') < имена.index('waterius_email')


def test_пустая_почта_требованием_не_становится() -> None:
    """Стенд без строки в файле не должен стирать почту устройству."""
    want = Stand.requirements(Стенд(Настройки('')))
    assert 'waterius_email' not in want


def test_облако_стенда_в_общих_требованиях() -> None:
    """Без него устройство слало бы в cloud.waterius.ru, и прогон зависел бы от интернета."""
    want = Stand.requirements(Стенд(Настройки('')))
    assert want['waterius_host'] == ОБЛАКО
    # Адрес прошивка принимает только при включённом получателе
    имена = list(want)
    assert имена.index('waterius_on') < имена.index('waterius_host')
