"""
Заводской сброс для тестов, которым нужно устройство «как из коробки».

`POST /api/reset` стирает настройки ЕСП, кроме токена, и перезагружает её в
режим настройки (`config.cpp`, factory_reset; init_config ставит SETUP_MODE):
точка портала поднимается снова, и в ней устройство уже без сети, без
приёмника и без веса импульса. Тестам сброса это предмет проверки, а «Авто» и
отпуску без веса - предусловие, которое настройками не создать.

Настройки attiny сбросом не стираются - это особенность платы. Единственное
исключение прошивка делает руками: типы входов она возвращает в NAMUR командой
`setCountersType` (`config.cpp`, factory_reset), потому что из ЕСП их иначе не
достать. Всё остальное, что живёт в attiny, сброс переживает.

Сам сброс занимает секунды. Работа - в возврате стенда: сеть, приёмник, брокер
и эталон настроек, - поэтому он собран здесь, а не повторён в каждом тесте.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING, Any

from loguru import logger

from . import portal as portal_mod

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .atboard import AtBoard  # а сбор тестов должен работать без них
    from .logwatch import Session
    from .stand import Stand

# Сколько ждать точку портала после сброса. Замеры 28.09: от ответа на
# `/api/reset` до строки о подъёме точки проходило 5, 6 и 7 секунд.
POST_RESET_AP_S = 30.0

# Сколько ждать первой строки ЕСП после сброса. Не пришла - ждать точку незачем:
# либо устройство не перезагрузилось, либо стенд ослеп, и обе беды видны сразу.
REBOOT_LOG_S = 5.0

# Канал точки портала после сброса. Точка встаёт на канал из настроек
# (`core/wifi.cpp`, ap_channel), а заводское `wifi_channel` - единица
# (`core/types.h`). Поэтому AT-плата, сидевшая в точке на прежнем канале,
# заводским сбросом выбивается всегда.
FACTORY_AP_CHANNEL = 1


def _rebooted(stand: Stand) -> bool:
    """Первая строка ЕСП после сброса: признак, что перезапуск состоялся."""
    deadline = time.monotonic() + REBOOT_LOG_S
    while True:
        stand.log.poll()
        if stand.log.lines:
            return True
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.2)


def wait_portal(cfg: Any, stand: Stand, board: AtBoard) -> str:
    """
    Дождаться точку портала после сброса двумя свидетелями сразу.

    METF видит, что ЕСП перезагрузилась и подняла точку; AT-плата видит ту же
    точку в эфире. Свидетели независимы - лог приезжает по сети с METF, скан по
    проводу с платы - и порознь каждый уже подводил: 28 сентября стенд две
    минуты ждал строку, которую сам же и стёр, а точка всё это время стояла.
    """
    assert _rebooted(stand), (
        f'за {REBOOT_LOG_S:.0f} с после сброса METF не отдала ни строки ЕСП: '
        f'устройство не перезагрузилось или стенд ослеп'
        f'{portal_mod.rescue_stray_portal(cfg, stand, board)[1]}')

    поднялась = None
    deadline = time.monotonic() + POST_RESET_AP_S
    while not поднялась and time.monotonic() < deadline:
        stand.log.poll()
        поднялась = portal_mod.find_ap_started(stand.log.lines)
        if not поднялась:
            time.sleep(0.5)
    assert поднялась, (
        f'портал не поднялся после сброса за {POST_RESET_AP_S:.0f} с'
        f'{portal_mod.rescue_stray_portal(cfg, stand, board)[1]}')
    ssid, канал = поднялась
    logger.info(f'METF: точка {ssid} поднялась на канале {канал}')

    сеть = portal_mod.ap_in_air(board, ssid)
    assert сеть, (f'METF видит точку {ssid} на канале {канал}, а AT-плата не '
                  f'находит её в эфире: один из свидетелей врёт')
    logger.info(f'эфир: точка {ssid} на канале {сеть.channel}, {сеть.rssi} дБм')
    assert сеть.channel == канал == FACTORY_AP_CHANNEL, (
        f'после сброса точка обязана стоять на канале {FACTORY_AP_CHANNEL} - '
        f'это заводское wifi_channel, - а стоит на {сеть.channel} по скану и '
        f'на {канал} по логу: либо настройки пережили сброс, либо точка берёт '
        f'канал не из них')
    return ssid


class FreshDevice:
    """Устройство после заводского сброса: сначала в своём портале, потом в сети стенда."""

    def __init__(self, cfg: Any, stand: Stand, board: AtBoard,
                 before: dict[str, Any]) -> None:
        self.cfg = cfg
        self.stand = stand
        self.board = board
        self.before = before      # посылка до сброса: токен и то, что вернуть после
        self.left = False         # выведено ли из портала в сеть стенда

    @classmethod
    def reset(cls, cfg: Any, stand: Stand) -> FreshDevice:
        """
        Сбросить и остаться в портале после перезагрузки.

        Посылка «до» нужна ради токена и почты. Берём последнюю, что стенд уже
        видел: отдельное нажатие стоило бы целого сеанса.
        """
        assert cfg.dut_password, 'пароль сети устройства в stand.ini'
        before = dict(stand.last_payload or {})
        if not before.get('key'):
            stand.reset_observers()
            stand.dut.press_button()
            before = stand.wait_session(timeout=180).payload or {}
        assert before.get('key'), 'в посылке нет токена: сверять сброс не с чем'

        board = portal_mod.open_portal(cfg, stand)
        try:
            # Окно чистим до запроса, а не после: перезапуск начинается через
            # доли секунды после ответа, и `clear()` вместе с кольцом METF
            # стирает загрузочный лог, которого сам же потом ждёт
            stand.log.clear()
            answer = board.post('/api/reset', portal_mod.HOST)
            assert answer.status == 200, f'/api/reset: код {answer.status}'
            logger.info('сброс отправлен, ждём перезапуск')

            ssid = wait_portal(cfg, stand, board)
            logger.info(f'портал после сброса: {ssid}, адрес платы {board.join(ssid)}')
            board.portal_ssid = ssid
        except BaseException:
            board.close()
            raise
        return cls(cfg, stand, board, before)

    def leave(self, timeout: float = 300.0) -> Session:
        """
        Вывести в сеть стенда: сеть, приёмник, выход из портала.

        Возвращает первый сеанс после сброса. Всё, чего настройка не касалась, -
        вес, период, типы входов, брокер - в нём ещё заводское.
        """
        self.stand.reset_observers()
        try:
            portal_mod.configure(self.board, self.stand.ap_ssid,
                                 self.cfg.dut_password, self.cfg.http_url)
        except Exception as err:
            # «Форма не дошла» и «потерялся ответ» выглядят одинаково, различает
            # их лог устройства: строки `parameter <имя>=` печатаются до ответа
            raise AssertionError(
                f'настройка портала после сброса не удалась: {err}\n'
                f'{self.stand.log.tail()}') from err
        self.left = True
        session = self.stand.wait_session(timeout=timeout)
        assert session.payload is not None, (
            f'после сброса и настройки посылка не дошла\n{session.text}')
        первая = session.payloads[0]
        logger.info(f'после сброса: f0={первая.get("f0")}, '
                    f'f1={первая.get("f1")}, '
                    f'period_min={первая.get("period_min")}')
        return session

    def close(self) -> None:
        """
        Вернуть стенд в рабочее состояние, чем бы ни кончился тест.

        Возврат обязан случиться и после упавшего `leave`: 28 сентября его
        пропуск оставил следующий тест без эталона, и W1 честно получил
        страницу определения счётчика вместо показаний.
        """
        беда = None
        try:
            if not self.left:
                self.leave()
        except BaseException as err:
            беда = err
        finally:
            self.board.close()
        try:
            restore(self.stand)
        except Exception as err:
            if беда is None:
                raise
            logger.warning(f'возврат стенда не удался: {err}')
        if беда is not None:
            raise беда


def restore(stand: Stand) -> None:
    """
    Вернуть стенд в рабочее состояние: требования по умолчанию и топик брокера.
    Сеть и приёмник к этому моменту уже вернул `leave`.
    """
    logger.info('возвращаем стенд в рабочее состояние после сброса')
    # Общие требования: часы платы, брокер, почта облака, эталон входов и
    # квитанций - всё это сброс вернул к умолчаниям прошивки. Осечка связи
    # здесь уносит не тест, а обстановку для всех следующих - поэтому с
    # повтором, как на подъёме
    from .conftest import bring_up
    bring_up(stand.ensure_requirements, 'требования после сброса')
    # Топик в лог не печатается, и сверить его можно только публикацией
    stand.ensure_mqtt()
