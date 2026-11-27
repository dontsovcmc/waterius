"""
Состояние устройства и требования теста - правила из state.py, без железа.
"""

from __future__ import annotations

from ..state import merge, same_value, unmet


def test_посылка_и_saved_перебивают_конфиг_загрузки() -> None:
    """
    Конфиг печатается при загрузке, до применения ответа сервера, - в сеансе
    с настройкой он старше посылки и строк `Saved:`.
    """
    state = merge(None,
                  config={'mqtt_on': '0', 'http_host': 'http://old/data'},
                  payload={'period_min': 120, 'email': 'a@b.c'},
                  saved={'mqtt_on': '1'})
    assert state['mqtt_on'] == '1'
    assert state['http_url'] == 'http://old/data', 'имя конфига -> имя параметра'
    assert state['waterius_email'] == 'a@b.c', 'имя посылки -> имя параметра'
    assert state['period_min'] == 120


def test_сеанс_без_посылки_не_стирает_известное() -> None:
    """Без сети посылки нет, но то, что стенд знал о настройках, остаётся."""
    before = {'period_min': 60, 'ctype1': 0}
    state = merge(before, config={'wifi_ssid': 'x'}, payload=None, saved={})
    assert state['period_min'] == 60
    assert state['wifi_ssid'] == 'x'


def test_невиданное_поле_считается_невыполненным() -> None:
    """Неизвестное значение - повод настроить, а не поверить."""
    assert unmet({'period_min': 120}, {'period_min': 120, 'ntp_server': 'h'}) == {
        'ntp_server': 'h'}


def test_требования_сравниваются_как_значения() -> None:
    """Флаги в посылке булевы, в конфиге строки, в требованиях числа."""
    state = {'vac': False, 'mqtt_on': '1', 'ch1': 10.0}
    assert unmet(state, {'vac': 0, 'mqtt_on': 1, 'ch1': '10.000'}) == {}
    assert same_value(True, 1)
    assert not same_value('0', 1)


def test_порядок_требований_сохраняется() -> None:
    """
    Получатель обязан уйти раньше адреса: адрес прошивка принимает только при
    включённом получателе, а параметры применяет в порядке ключей.
    """
    want = {'mqtt_on': 1, 'mqtt_host': 'h', 'period_min': 120}
    assert list(unmet({}, want)) == ['mqtt_on', 'mqtt_host', 'period_min']
