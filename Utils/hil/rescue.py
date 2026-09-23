"""
Вернуть стенд в рабочее состояние одной командой.

Упавший или убитый прогон оставляет стенд там, куда его увёл тест: точка
доступа погашена, в фильтре роутера лежит правило, линии METF прижаты к земле,
радио AT-платы ищет пропавшую точку и греет модуль. Следующий прогон тогда
падает по чужой вине, и разбор начинается с чужого мусора.

    python3 Utils/hil/rescue.py             # починить
    python3 Utils/hil/rescue.py --check     # только доложить, ничего не трогать
"""

from __future__ import annotations

import argparse
import socket

from loguru import logger

from . import config
from .atboard import AtBoard
from .dut import Dut
from .metf import Metf
from .router import connect


def _router(cfg: config.StandConfig, check: bool) -> list[str]:
    """Снять правила фильтра и поднять точку: без них стенд слеп и нем."""
    router = connect(cfg.router_port or None, cfg.router_host or None,
                     cfg.router_password, cfg.ap_password)
    try:
        # version() отвечает многострочным баннером - в отчёт берём первую строку
        banner = router.version().strip().splitlines()[0]
        done = [f'роутер: {banner}, канал {router.config().get("channel", "?")}']
        rules = sum(len(router.acl_rules(name)) for name in ('block', 'allow'))
        ap_on = router.ap_enabled()
        if check:
            done.append(f'роутер: правил фильтра {rules}, точка {"поднята" if ap_on else "погашена"}')
            return done
        if rules:
            router.acl_clear()
            done.append(f'роутер: снято правил фильтра - {rules}')
        if ap_on is not True:
            router.ap(True)
            done.append('роутер: точка доступа поднята')
        return done
    finally:
        router.close()


def _metf(cfg: config.StandConfig, check: bool) -> list[str]:
    """Отпустить линии: прижатый вход даёт чужие импульсы весь прогон."""
    api = Metf(cfg.metf_host)
    api.ping()
    if check:
        return [f'METF {cfg.metf_host}: отвечает']
    Dut(api, cfg.button_pin, cfg.ch0_pin, cfg.ch1_pin, cfg.reset_pin).init()
    return [f'METF {cfg.metf_host}: линии отпущены']


def _atboard(cfg: config.StandConfig, check: bool) -> list[str]:
    """Погасить радио: иначе плата вечно ищет точку портала и греется."""
    if not cfg.atboard_port:
        return ['AT-плата: не задана в stand.ini']
    if check:
        return [f'AT-плата: {cfg.atboard_port}']
    board = AtBoard(cfg.atboard_port)
    board.close()                      # close() и есть парковка радио
    return [f'AT-плата {cfg.atboard_port}: радио погашено']


def _ports(cfg: config.StandConfig) -> list[str]:
    """
    Чужой процесс на порту приёмника - готовое падение прогона на `Errno 48`.

    Стенд делит машину с соседними проектами, и занятый порт надо назвать до
    прогона, а не узнавать из трассировки на первом же тесте.
    """
    busy = []
    for name, port in (('приёмник', cfg.receiver_port),
                       ('приёмник https', cfg.receiver_tls_port),
                       ('брокер', cfg.broker_port)):
        with socket.socket() as probe:
            probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                probe.bind(('0.0.0.0', port))
            except OSError:
                busy.append(f'порт {port} ({name}) занят чужим процессом')
    return busy or ['порты приёмника и брокера свободны']


def main() -> None:
    parser = argparse.ArgumentParser(description='Вернуть стенд в рабочее состояние')
    parser.add_argument('--config', default=None, help='путь к stand.ini')
    parser.add_argument('--check', action='store_true',
                        help='только доложить состояние, ничего не менять')
    args = parser.parse_args()

    cfg = config.load(args.config)
    lines: list[str] = []
    for step in (_router, _metf, _atboard):
        try:
            lines += step(cfg, args.check)
        except Exception as err:                       # доходим до конца: чинится что чинится
            lines.append(f'{step.__name__.strip("_")}: не вышло - {err}')
    lines += _ports(cfg)
    for line in lines:
        logger.info(line)


if __name__ == '__main__':
    main()
