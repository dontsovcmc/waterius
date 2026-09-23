"""
Заводской сброс для тестов, которым нужно устройство «как из коробки».

`POST /api/reset` стирает настройки ЕСП, кроме токена, и перезагружает её в
режим настройки (`config.cpp`, factory_reset; init_config ставит SETUP_MODE):
точка портала поднимается снова, и в ней устройство уже без сети, без
приёмника и без веса импульса. Тестам сброса это предмет проверки, а «Авто» и
отпуску без веса - предусловие, которое настройками не создать.

Сам сброс занимает секунды. Работа - в возврате стенда: сеть, приёмник, брокер
и эталон настроек, - поэтому он собран здесь, а не повторён в каждом тесте.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from loguru import logger

from . import portal as portal_mod

if TYPE_CHECKING:                 # Stand тянет pyserial и paho-mqtt,
    from .atboard import AtBoard  # а сбор тестов должен работать без них
    from .logwatch import Session
    from .stand import Stand

# Перезагрузка после сброса и подъём той же точки заново
POST_RESET_AP_S = 120.0


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
        assert cfg.ap_password, '[router] ap_password в stand.ini'
        before = dict(stand.last_payload or {})
        if not before.get('key'):
            stand.reset_observers()
            stand.dut.press_button()
            before = stand.wait_session(timeout=180).payload or {}
        assert before.get('key'), 'в посылке нет токена: сверять сброс не с чем'

        board = portal_mod.open_portal(cfg, stand)
        try:
            answer = board.post('/api/reset', portal_mod.HOST)
            assert answer.status == 200, f'/api/reset: код {answer.status}'
            logger.info('сброс отправлен, ждём перезапуск')

            # Точка та же, но ассоциация AT-платы умерла вместе с перезапуском
            stand.log.clear()
            ssid = portal_mod.wait_ap(stand, POST_RESET_AP_S)
            assert ssid, f'портал не поднялся после сброса за {POST_RESET_AP_S:.0f} с'
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
        portal_mod.configure(self.board, self.stand.ap_ssid, self.cfg.ap_password,
                             self.cfg.http_url)
        self.left = True
        session = self.stand.wait_session(timeout=timeout)
        assert session.payload is not None, (
            f'после сброса и настройки посылка не дошла\n{session.text}')
        logger.info(f'после сброса: f0={session.payload.get("f0")}, '
                    f'f1={session.payload.get("f1")}, '
                    f'period_min={session.payload.get("period_min")}')
        return session

    def close(self) -> None:
        """Вернуть стенд в рабочее состояние, чем бы ни кончился тест."""
        try:
            if not self.left:
                self.leave()
        finally:
            self.board.close()
        restore(self.stand, self.before)


def restore(stand: Stand, before: dict[str, Any]) -> None:
    """
    Вернуть стенд в рабочее состояние: почта, требования по умолчанию, топик
    брокера. Сеть и приёмник к этому моменту уже вернул `leave`.
    """
    logger.info('возвращаем стенд в рабочее состояние после сброса')
    # Почта уезжает в облако вместе с показаниями, и без неё блок G сверял бы
    # ответ облака, а не прошивку. Имя параметра - waterius_email: в посылке
    # поле называется короче (json.cpp), и по нему прошивка его не примет.
    email = before.get('email')
    if email:
        try:
            stand.setup(waterius_email=email)
        except AssertionError as err:
            logger.warning(f'почта не вернулась: {err}')

    # Общие требования: часы платы, брокер, эталон входов и квитанций. Всё это
    # сброс вернул к умолчаниям прошивки
    stand.ensure_requirements()
    # Топик в лог не печатается, и сверить его можно только публикацией
    stand.ensure_mqtt()
