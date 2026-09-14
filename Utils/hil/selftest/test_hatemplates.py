"""
Проверка шаблонов автодискавери - без брокера и без устройства.

Шаблоны здесь - те же строки, что собирает `ha/discovery_entity.cpp`. Смысл
проверок - что стенд ловит именно те поломки, ради которых I15 заведён: чужой
канал в шаблоне (#288, #319), булево без приведения к 1/0 (#419), поле, которого
нет в посылке.
"""

from __future__ import annotations

import pytest

from ..hatemplates import TemplateError, problems, render

PAYLOAD = {'ch0': 12.345, 'ch1': 7.5, 'imp0': 120, 'vac': True, 'alarm_wet1': False,
           'ctype0': 0, 'ctype1': 255, 'timestamp': '2026-09-15T10:00:00+0000'}


def sensor(entity_id: str) -> dict:
    return {'val_tpl': '{{ value_json.' + entity_id + ' | is_defined }}'}


def flag(entity_id: str) -> dict:
    return {'val_tpl': '{{ value_json.' + entity_id + ' | is_defined | int }}'}


CTYPE_OPTIONS = ['MECHANIC', 'ELECTRONIC', 'ELECTRONIC_HIGH', 'LEAKAGE', 'LEAKAGE_NC',
                 'NOT_USED']


def ctype(entity_id: str) -> dict:
    return {
        'options': CTYPE_OPTIONS,
        'val_tpl': (f'{{% if value_json.{entity_id}==0 %}} MECHANIC '
                    f'{{% elif value_json.{entity_id}==2 %}} ELECTRONIC '
                    f'{{% elif value_json.{entity_id}==4 %}} ELECTRONIC_HIGH '
                    f'{{% elif value_json.{entity_id}==5 %}} LEAKAGE '
                    f'{{% elif value_json.{entity_id}==6 %}} LEAKAGE_NC '
                    f'{{% elif value_json.{entity_id}==255 %}} NOT_USED '
                    f'{{% endif %}}'),
    }


def test_sensor_reads_its_channel() -> None:
    assert problems('sensor', 'ch0', sensor('ch0'), PAYLOAD) == []


def test_swapped_channel_is_caught() -> None:
    found = problems('sensor', 'ch0', sensor('ch1'), PAYLOAD)
    assert found and 'не своё поле' in found[0]


def test_flag_renders_as_one_or_zero() -> None:
    switch = dict(flag('vac'), stat_on='1', stat_off='0')
    assert render(switch['val_tpl'], PAYLOAD) == '1'
    assert problems('switch', 'vac', switch, PAYLOAD) == []


def test_flag_without_int_is_caught() -> None:
    binary = dict(sensor('alarm_wet1'), pl_on='1', pl_off='0')
    found = problems('binary_sensor', 'alarm_wet1', binary, PAYLOAD)
    assert found and "'False'" in found[0]


def test_select_renders_an_option() -> None:
    assert render(ctype('ctype1')['val_tpl'], PAYLOAD) == 'NOT_USED'
    assert problems('select', 'ctype0', ctype('ctype0'), PAYLOAD) == []


def test_missing_field_is_caught() -> None:
    with pytest.raises(TemplateError):
        render('{{ value_json.nosuch | is_defined }}', PAYLOAD)
    assert problems('sensor', 'nosuch', sensor('nosuch'), PAYLOAD)


def test_attributes_must_form_json() -> None:
    good = dict(sensor('ch0'), json_attributes_template=(
        '{"Impulses":"{{ value_json.imp0 | is_defined }}"}'))
    assert problems('sensor', 'ch0', good, PAYLOAD) == []

    broken = dict(sensor('ch0'), json_attributes_template=(
        '{"Impulses":"{{ value_json.imp9 | is_defined }}"}'))
    assert problems('sensor', 'ch0', broken, PAYLOAD)


def test_button_has_no_state() -> None:
    assert problems('button', 'arst', {'cmd_t': 'x/arst/set'}, PAYLOAD) == []
