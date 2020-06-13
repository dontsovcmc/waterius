# -*- coding: utf-8 -*-
import unittest
from metf_python_client import log, METFClient, LOW, HIGH, INPUT, OUTPUT, INPUT_PULLUP, str2hex, hex2str

from waterius import WateriusAttiny_13


ESP_HOST = '192.168.3.46'


class TestGetHeader(unittest.TestCase):

    def setUp(self):
        log.info('ESP: ' + ESP_HOST)
        self.api = METFClient(ESP_HOST)
        self.w = WateriusAttiny_13(self.api)

        self.w.start_i2c()

    def test_header(self):
        try:
            # имитируем нажатие кнопки, ждем HIGH
            self.w.wake_up()

            assert self.w.get_mode() == self.w.TRANSMIT_MODE

            header = self.w.get_header()

            #self.assertEqual(self.w.ATTINY_VER, header.version)
            self.assertGreater(header.voltage, 2700)
            self.assertLess(header.voltage, 3500)
            self.assertGreater(header.resets, 0)

            self.w.send_sleep()
            self.w.wait_off()

        finally:
            self.w.manual_turn_off()

    def test_counter(self):
        try:
            self.w.wake_up()

            self.w.start_i2c()

            assert self.w.get_mode() == self.w.TRANSMIT_MODE

            header = self.w.get_header()

            self.w.send_sleep()

            log.info('ESP: impules0={}'.format(header.impulses0))
            log.info('ESP: impules1={}'.format(header.impulses1))

            # Ждем когда снимет питание
            self.w.wait_off()

            log.info('ESP -- impulses --- ')

            self.w.impuls(0)

            self.w.wake_up()

            self.api.i2c_begin(self.w.sda, self.w.sdl)

            assert self.w.get_mode() == self.w.TRANSMIT_MODE

            header2 = self.w.get_header()

            self.w.send_sleep()
            self.w.wait_off()

            log.info('ESP: header 2')
            log.info('ESP: impules0={}'.format(header2.impulses0))
            log.info('ESP: impules1={}'.format(header2.impulses1))

            assert header2.impulses0 == header.impulses0 + 1
            #assert header2.impulses1 == header.impulses1 + 1

        finally:
            self.w.manual_turn_off()

    def test_counter_while_transmitting(self):
        """
        Включаем, запоминаем текущие показания
        Делаем 10 импульсов
        Выключаем
        Включаем, смотрим, увеличились ли на 10
        :return:
        """
        try:
            self.w.wake_up()

            self.w.start_i2c()

            header = self.w.get_header()

            log.info('ESP: impules0={}'.format(header.impulses0))
            log.info('ESP: impules1={}'.format(header.impulses1))

            log.info('ESP -- impulses --- ')

            impulses = 10
            for i in range(0, impulses):
                self.w.impuls(0)

            self.w.send_sleep()

            # Ждем когда снимет питание
            self.w.wait_off()

            self.api.delay(2000)

            self.w.wake_up()

            self.api.i2c_begin(self.w.sda, self.w.sdl)

            assert self.w.get_mode() == self.w.TRANSMIT_MODE

            header2 = self.w.get_header()

            self.w.send_sleep()
            self.w.wait_off()

            log.info('ESP: header 2')
            log.info('ESP: impules0={}'.format(header2.impulses0))
            log.info('ESP: impules1={}'.format(header2.impulses1))

            assert header2.impulses0 == header.impulses0 + impulses

        finally:
            self.w.manual_turn_off()


if __name__ == '__main__':
    unittest.main()
