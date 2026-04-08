import sys
import time
import argparse
from loguru import logger
from metf_python_client import METFClient

from helpers import SerialBuffer, assert_wait_log
from metf_python_client.boards import esp32c6_super_mini as board

BUTTON_PIN = board.GPIO1
CH1_PIN = board.GPIO2
CH0_PIN = board.GPIO3
RESET_PIN = board.GPIO0
COLOR_BLUE = '0000FF'
COLOR_GREEN = '00FF00'
COLOR_RED = 'FF0000'
COLOR_VIOLET = '800080'


def pause(msec: int = 100) -> None:
    time.sleep(msec / 1000.0)


def blynk(color: str, msec: int = 50) -> None:
    api.rgb_brightness(50)
    api.rgb_color(color)
    pause(msec)
    api.rgb_brightness(0)


def init_board(api: METFClient) -> None:
    api.rgb_begin()
    api.rgb_brightness(0)
    blynk(COLOR_BLUE)

    api.pinMode(RESET_PIN, board.INPUT)
    api.pinMode(CH0_PIN, board.INPUT)
    api.pinMode(CH1_PIN, board.INPUT)
    api.pinMode(BUTTON_PIN, board.INPUT)


def press_low(api: METFClient, pin: int, msec: int = 100) -> None:
    api.pinMode(pin, board.OUTPUT)
    api.digitalWrite(pin, board.LOW)
    pause(msec)
    api.pinMode(pin, board.INPUT)


def factory_test(args: list[str]):
    """
    Тест на производстве
    :return:
    """
    try:

        api = METFClient(args.host)
        buffer = SerialBuffer(api)
        api.serial_begin()

        init_board(api)

        start = time.time()
        #while time.time() - start < 15:
        #    print(api.serial_readlines(wait=5000, delimiter='\n', prefix='00:'))

        press_low(api, RESET_PIN)
        pause(200)
        blynk(COLOR_VIOLET)

        press_low(api, CH0_PIN, 500)
        press_low(api, CH1_PIN, 500)

        blynk(COLOR_VIOLET)
        pause(1000)

        press_low(api, CH0_PIN, 500)
        press_low(api, CH1_PIN, 500)

        blynk(COLOR_VIOLET)
        press_low(api, BUTTON_PIN, 100)

        #assert_wait_log(buffer, r'boot', 3.0, 'Точно плата работает?')

        blynk(COLOR_VIOLET)
        assert_wait_log(buffer, r'imp0:(\d+)', 5.0, 'Не работает вход 0', lambda x: x >= 0.001)

        blynk(COLOR_VIOLET)
        assert_wait_log(buffer, r'imp1:(\d+)', 5.0, 'Не работает вход 1', lambda x: x >= 0.001)

        blynk(COLOR_VIOLET)
        assert_wait_log(buffer, r'Connected', 25.0, 'Ошибка подключения к wifi')

        blynk(COLOR_VIOLET)
        assert_wait_log(buffer, r'Send OK', 5.0, 'Ошибка отправки')

        logger.info(f'Test success, time: {time.time() - start:.1f}')

        blynk(COLOR_GREEN, 2000)

    except Exception as err:
        logger.info(f'Ошибка: {err}')
    sys.exit(0)


if __name__ == '__main__':
    ESP_HOST = '192.168.51.250'
    parser = argparse.ArgumentParser(description='Factory test for Waterius')
    parser.add_argument('--host', type=str, default=ESP_HOST, help=f'IP address of METFClient (default: {ESP_HOST})')
    args = parser.parse_args()

    api = METFClient(args.host)

    factory_test(args)
