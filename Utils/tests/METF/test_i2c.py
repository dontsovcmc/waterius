# -*- coding: utf-8 -*-
import unittest
from metf_python_client import METFClient

from waterius import WateriusClassic


ESP_HOST = '192.168.3.46'

MANUAL_PRESS_BUTTON = 10.0


class TestGetHeader(unittest.TestCase):

    def setUp(self):
        self.api = METFClient(ESP_HOST)
        self.w = WateriusClassic(self.api)

        self.w.start_i2c()

    def test_header(self):
        try:
            # имитируем нажатие кнопки, ждем HIGH
            self.w.wake_up(MANUAL_PRESS_BUTTON)

            assert self.w.get_mode() == self.w.TRANSMIT_MODE

            header = self.w.get_header()

            #self.assertEqual(self.w.ATTINY_VER, header.version)
            self.assertGreater(header.voltage, 2700)
            self.assertLess(header.voltage, 3500)

            self.w.send_sleep()
            self.w.wait_off()

        finally:
            self.w.manual_turn_off()


if __name__ == '__main__':
    unittest.main()
