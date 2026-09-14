"""
Состояние устройства, как его видит стенд, и требования теста к нему.

Стенд помнит последнее известное состояние и перенастраивает устройство, только
если требования теста с ним расходятся: сеанс с настройкой на живом железе идёт
десятки секунд, и платить их за тест, которому всё и так подходит, незачем.

Состояние собирается из трёх источников одного сеанса, по старшинству:

- настройки, напечатанные при загрузке (`Session.config`) - до применения
  того, что прислал сервер;
- посылка - после применения;
- строки `Saved:` - тоже после, и единственный след для адресов, портов и
  включённости получателей, которых в посылке нет (`json.cpp`).

Модуль без зависимостей стенда: правила проверяются в selftest без железа.
"""

from __future__ import annotations

from typing import Any, Mapping

# Поля посылки и конфига, названные не так, как параметр прошивки. Требования
# пишутся именами параметров - теми, что уходят в ответе сервера.
PAYLOAD_NAMES = {'email': 'waterius_email'}
CONFIG_NAMES = {'http_host': 'http_url'}


def same_value(got: Any, want: Any) -> bool:
    """
    Одно ли это значение настройки.

    Флаги задаются числом (в прошивку они и уходят как "1"/"0"), а в посылке
    приезжают булевыми, поэтому сравнение строк дало бы вечное '1' != 'True'.
    Числа сравниваем как числа: показания задаются с литрами ("10.000"), а в
    посылке приезжают числом 10.0.
    """
    if isinstance(got, bool) or isinstance(want, bool):
        return bool(got) == bool(want)
    try:
        return abs(float(got) - float(want)) < 1e-6
    except (TypeError, ValueError):
        return str(got) == str(want)


def merge(state: Mapping[str, Any] | None, config: Mapping[str, str],
          payload: Mapping[str, Any] | None,
          saved: Mapping[str, str]) -> dict[str, Any]:
    """Состояние после сеанса: поверх прежнего - конфиг, посылка, `Saved:`."""
    out = dict(state or {})
    for name, value in config.items():
        out[CONFIG_NAMES.get(name, name)] = value
    for name, value in (payload or {}).items():
        out[PAYLOAD_NAMES.get(name, name)] = value
    out.update(saved)
    return out


def unmet(state: Mapping[str, Any], want: Mapping[str, Any]) -> dict[str, Any]:
    """
    Требования, которые состояние не выполняет, в порядке требований.

    Поле, которого стенд не видел, - тоже невыполненное: полагаться на
    неизвестное значение значит однажды проверять не то устройство.
    """
    return {name: value for name, value in want.items()
            if name not in state or not same_value(state[name], value)}
