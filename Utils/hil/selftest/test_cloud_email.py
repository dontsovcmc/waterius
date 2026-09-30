"""
Почта учётной записи облака - без железа.

Прогон 29.09: облако ответило 404 на все 662 посылки, и пять тестов блока G
говорили о его ответе вместо прошивки. Почта у устройства была пуста, а стенд
её не требовал - возвращал только после заводского сброса, из посылки.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from .. import config as stand_config
from ..conftest import live_cloud
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


class Стенд:
    """Стенд, у которого спрашивают только требования к устройству."""

    mqtt = None

    def __init__(self, cfg: Any) -> None:
        self.cfg = cfg


class Настройки:
    """Часть stand.ini, которую читает Stand.requirements."""

    metf_host = '192.168.100.21'
    http_url = 'http://192.168.100.18:8010/'

    def __init__(self, email: str, nocloud: bool = False) -> None:
        self.dut_email = email
        self.nocloud = nocloud


class Прогон:
    """Запрос фикстуры: ключи командной строки и stand.ini."""

    def __init__(self, nocloud: bool = False, stand: bool = True,
                 email: str = 'test@waterius.ru') -> None:
        self.config = self
        self._keys = {'--nocloud': nocloud, '--stand': stand}
        self._cfg = Настройки(email)

    def getoption(self, name: str) -> bool:
        return self._keys[name]

    def getfixturevalue(self, name: str) -> Any:
        assert name == 'cfg'
        return self._cfg


# Фикстуру зовут как обычную функцию: pytest оставляет её под __wrapped__.
спросить = live_cloud.__wrapped__


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


def test_облако_включено_и_смотрит_на_заводской_адрес() -> None:
    """Чужой адрес, оставшийся в устройстве, иначе пережил бы все прогоны."""
    want = Stand.requirements(Стенд(Настройки('')))
    assert want['waterius_on'] == 1
    assert want['waterius_host'] == 'https://cloud.waterius.ru'
    # Адрес прошивка принимает только при включённом получателе
    имена = list(want)
    assert имена.index('waterius_on') < имена.index('waterius_host')


def test_прогон_без_облака_выключает_его_тумблер() -> None:
    """Свой сервер - отдельный получатель, и его `--nocloud` не касается."""
    want = Stand.requirements(Стенд(Настройки('test@waterius.ru', nocloud=True)))
    assert want['waterius_on'] == 0 and want['http_on'] == 1
    # Выключенный получатель адрес и почту не принимает
    assert 'waterius_host' not in want and 'waterius_email' not in want


def test_прогон_без_облака_такой_тест_пропускает() -> None:
    with pytest.raises(pytest.skip.Exception, match='--nocloud'):
        спросить(Прогон(nocloud=True))


def test_с_облаком_тест_идёт_как_обычно() -> None:
    assert спросить(Прогон()) is None


def test_стенд_без_почты_тест_не_гоняет() -> None:
    """Молча сверять ответ облака чужой учётной записи хуже, чем не гонять."""
    with pytest.raises(pytest.skip.Exception, match='email'):
        спросить(Прогон(email=''))


def test_без_стенда_фикстура_не_спрашивает_настроек() -> None:
    """Сбор тестов без `--stand` идёт на машине, где stand.ini может не быть."""
    assert спросить(Прогон(stand=False, email='')) is None
