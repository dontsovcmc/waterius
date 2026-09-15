"""
Шаблоны автодискавери, отрендеренные по посылке, - то, что увидит Home Assistant.

Сущность прошивка объявляет шаблоном над корневой посылкой (`ha/discovery_entity.cpp`,
val_tpl). Ошибка в шаблоне не видна ни в логе, ни в брокере: сущность молча
показывает «неизвестно» или берёт значение соседнего канала (#288, #319), а
переключатель с `True` вместо `1` не совпадает со своим `stat_on` (#419).

Рендер - jinja2 с фильтром `is_defined`: в jinja2 его нет, он из Home Assistant.
Модуль без зависимостей стенда: правила проверяются в selftest без железа.
"""

from __future__ import annotations

import json
from typing import Any

import jinja2


class TemplateError(Exception):
    pass


def _is_defined(value: Any) -> Any:
    """Фильтр Home Assistant: значение поля или ошибка, если поля нет."""
    if isinstance(value, jinja2.Undefined):
        raise TemplateError(f'поля нет в посылке: {value._undefined_name}')
    return value


_env = jinja2.Environment(autoescape=False)
_env.filters['is_defined'] = _is_defined


def render(template: str, payload: dict[str, Any]) -> str:
    """
    Отрендерить шаблон по посылке.

    Пробелы по краям отрезаются: шаблоны выбора печатают значение в пробелах
    (`{% if ... %} MECHANIC {% elif ...`), а сверяется само значение.
    """
    try:
        text = _env.from_string(template).render(value_json=payload,
                                                  value=json.dumps(payload))
    except jinja2.TemplateError as err:
        raise TemplateError(f'{template!r}: {err}') from err
    return text.strip()


def _same(rendered: str, value: Any) -> bool:
    if isinstance(value, bool):
        return rendered == str(value)
    if isinstance(value, (int, float)):
        try:
            return abs(float(rendered) - float(value)) < 1e-6
        except ValueError:
            return False
    return rendered == str(value)


def problems(entity_type: str, entity_id: str, entity: dict[str, Any],
             payload: dict[str, Any]) -> list[str]:
    """Что не так с одной сущностью автодискавери при этой посылке."""
    found: list[str] = []
    where = f'{entity_type}/{entity_id}'

    attributes = entity.get('json_attributes_template')
    if attributes:
        try:
            json.loads(render(attributes, payload))
        except (TemplateError, ValueError) as err:
            found.append(f'{where}: атрибуты не собираются в JSON: {err}')

    template = entity.get('val_tpl')
    if template is None:
        return found                  # кнопка: состояния нет, шаблона тоже

    # Канальная сущность обязана читать своё поле: перепутанный канал
    # отрендерился бы без ошибки, числом соседа
    if entity_id[-1:] in ('0', '1') and f'value_json.{entity_id}' not in template:
        found.append(f'{where}: шаблон читает не своё поле: {template}')

    try:
        value = render(template, payload)
    except TemplateError as err:
        return found + [f'{where}: {err}']

    allowed = {
        'binary_sensor': (entity.get('pl_on'), entity.get('pl_off')),
        'switch': (entity.get('stat_on'), entity.get('stat_off')),
        'select': tuple(entity.get('options') or ()),
    }.get(entity_type)
    if allowed is not None:
        if value not in allowed:
            found.append(f'{where}: значение {value!r} не из {allowed}')
    elif entity_id in payload and not _same(value, payload[entity_id]):
        found.append(f'{where}: {value!r}, а в посылке {payload[entity_id]!r}')
    return found
