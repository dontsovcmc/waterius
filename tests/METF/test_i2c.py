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


if __name__ == '__main__':
    unittest.main()
