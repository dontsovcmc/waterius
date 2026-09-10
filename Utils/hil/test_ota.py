"""
Обновление по воздуху - блок J ручного плана.

Команду обновления прошивка получает полем `ota` в ответе сервера, поэтому
запускает её стенд сам, без облака: приёмник кладёт в ответ адреса образов и
тут же их раздаёт. Раздаёт по https - образы качает `WiFiClientSecure`
(`ota_update.cpp`), обычный порт для этого не годится.

Образы берутся из дерева, из каталога сборки. Полное обновление идёт, только
когда версия в устройстве совпадает с версией в дереве: тогда после
обновления в устройстве оказывается ровно то, что там было, и стенд остаётся
на известной прошивке. Иначе тест пропускается - подсунуть железу чужую
сборку и не иметь чем вернуть хуже, чем не проверить.

Отказы проверяются без риска для флеша: битый адрес прошивка отвергает до
записи, а просевшее питание - до загрузки.

Обновление есть только у Ватериуса-2 (`main.cpp`, WATERIUS_MODEL_2).
"""

from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Any

import pytest
from loguru import logger

from . import portal as portal_mod

pytestmark = [pytest.mark.stand, pytest.mark.slow]

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'ESP8266' / '.pio' / 'build' / 'waterius_2'

MODEL_2 = 2

# ota_update.h
OTA_MIN_VOLTAGE_MV = 3300

# core/types.h, enum OtaError
OTA_ERR_NONE = 0
OTA_ERR_FW_UPDATE = 3
OTA_ERR_LOW_BATTERY = 4


def image(name: str) -> bytes:
    path = BUILD / name
    if not path.exists():
        pytest.skip(f'нет образа {path}: соберите `pio run -d ESP8266 -e waterius_2`')
    return path.read_bytes()


def part(path: str, data: bytes, cfg: Any) -> dict[str, Any]:
    """Часть команды обновления: адрес, контрольная сумма и размер образа."""
    return {'url': cfg.https_file(path),
            'md5': hashlib.md5(data).hexdigest(),
            'size': len(data)}


@pytest.fixture(autouse=True)
def model_2(stand: Any) -> None:
    payload = stand.last_payload
    if payload is not None and int(payload.get('model', MODEL_2)) != MODEL_2:
        pytest.skip('обновление по воздуху есть только у Ватериуса-2')


@pytest.fixture
def images(stand: Any, cfg: Any) -> dict[str, dict[str, Any]]:
    """Образы из дерева, разложенные приёмником по https."""
    stand.receiver.start_tls(cfg.receiver_tls_port)
    firmware = image('firmware.bin')
    filesystem = image('littlefs.bin')
    stand.receiver.serve('/ota/firmware.bin', firmware)
    stand.receiver.serve('/ota/littlefs.bin', filesystem)
    return {'firmware': part('/ota/firmware.bin', firmware, cfg),
            'filesystem': part('/ota/littlefs.bin', filesystem, cfg)}


def request_ota(stand: Any, ota: dict[str, Any]) -> None:
    """Положить команду обновления в ответ на следующую посылку и разбудить."""
    stand.reset_observers()
    stand.receiver.reply_settings({'ota': ota})
    stand.dut.press_button()


def test_J1_ota_updates_both_images(stand: Any, cfg: Any,
                                    images: dict[str, Any]) -> None:
    """
    Полное обновление: сначала файловая система, потом прошивка, потом
    перезагрузка. После неё устройство выходит на связь само.

    Версия в устройстве и в дереве обязаны совпадать - иначе стенд остался бы
    на сборке, которой нет в репозитории.
    """
    want = portal_mod.tree_version(ROOT)
    if want is None or stand.esp_version != want:
        pytest.skip(f'на устройстве {stand.version_str}, в дереве '
                    f'{".".join(map(str, want)) if want else "?"} - '
                    'обновлять нечем, вернуть было бы нечем тоже')

    payload = stand.last_payload or {}
    voltage = float(payload.get('voltage', 0))
    if 0 < voltage < OTA_MIN_VOLTAGE_MV:
        pytest.skip(f'питание {voltage:.0f} мВ ниже {OTA_MIN_VOLTAGE_MV}: '
                    'прошивка откажется обновляться, и правильно сделает')

    request_ota(stand, images)

    assert stand.log.wait_line('OTA: start', timeout=180) is not None, (
        'прошивка не увидела команду обновления')
    assert stand.log.wait_line('OTA: filesystem updated OK', timeout=600) is not None
    assert stand.log.wait_line('OTA: firmware updated OK', timeout=900) is not None
    assert stand.log.wait_line('OTA: complete, restarting ESP', timeout=60) is not None

    # После перезагрузки устройство доигрывает обычный сеанс
    session = stand.wait_session(timeout=300)
    assert session.esp_version == want, (
        f'после обновления версия {session.esp_version}, ждали {want}')
    assert session.payload is not None
    assert session.payload['ota_error'] == OTA_ERR_NONE, session.payload['ota_error']

    logger.info('образы вернули устройству ту же версию, что была')


def test_J4_bad_url_is_reported(stand: Any, cfg: Any) -> None:
    """
    Битый адрес образа: обновление не состоялось, ошибка уезжает в посылке и
    сбрасывается после публикации.

    Флеш при этом не трогается - загрузка не начинается вовсе, поэтому тест
    безопасен на любой прошивке.
    """
    stand.receiver.start_tls(cfg.receiver_tls_port)
    ota = {'firmware': {'url': cfg.https_file('/ota/no-such-image.bin'),
                        'md5': '0' * 32, 'size': 1024}}

    request_ota(stand, ota)

    assert stand.log.wait_line('OTA: firmware update failed', timeout=300) is not None, (
        'прошивка не сообщила о неудаче обновления')

    stand.reset_observers()
    stand.dut.press_button()
    reported = stand.wait_session(timeout=180)
    assert reported.payload is not None
    assert reported.payload['ota_error'] == OTA_ERR_FW_UPDATE, reported.payload

    stand.reset_observers()
    stand.dut.press_button()
    cleared = stand.wait_session(timeout=180)
    assert cleared.payload is not None
    assert cleared.payload['ota_error'] == OTA_ERR_NONE, (
        'ошибка обновления обязана сбрасываться после публикации')


def test_J3_low_battery_refuses(stand: Any, cfg: Any) -> None:
    """
    На просевших батарейках обновление не начинается: прерванная запись флеша
    оставила бы устройство без прошивки.

    Проверить это можно только на устройстве, которое действительно питается
    ниже порога: напряжением стенд не управляет.
    """
    payload = stand.last_payload or {}
    voltage = float(payload.get('voltage', 0))
    if voltage >= OTA_MIN_VOLTAGE_MV:
        pytest.skip(f'питание {voltage:.0f} мВ выше {OTA_MIN_VOLTAGE_MV} мВ: '
                    'отказ по батарейкам так не воспроизвести, см. 04_not-tested.md')

    stand.receiver.start_tls(cfg.receiver_tls_port)
    ota = {'firmware': {'url': cfg.https_file('/ota/no-such-image.bin'),
                        'md5': '0' * 32, 'size': 1024}}
    request_ota(stand, ota)

    assert stand.log.wait_line('OTA: voltage too low', timeout=180) is not None

    stand.reset_observers()
    stand.dut.press_button()
    reported = stand.wait_session(timeout=180)
    assert reported.payload['ota_error'] == OTA_ERR_LOW_BATTERY, reported.payload
