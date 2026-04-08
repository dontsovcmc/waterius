# -*- coding: utf-8 -*-
import unittest
from metf_python_client import log, METFClient, LOW, HIGH, INPUT, OUTPUT, INPUT_PULLUP, str2hex, hex2str

from waterius import Waterius4C2W, WATERIUS_4C2W


ESP_HOST = '192.168.3.46'

MANUAL_PRESS_BUTTON = 10.0


class TestGetHeader(unittest.TestCase):

    def setUp(self):
        self.api = METFClient(ESP_HOST)
        self.w = Waterius4C2W(self.api)

        self.w.start_i2c()

    def test_header(self):
        try:
            # имитируем нажатие кнопки, ждем HIGH
            self.w.wake_up(MANUAL_PRESS_BUTTON)

            assert self.w.get_mode() == self.w.TRANSMIT_MODE

            header = self.w.get_header()

            self.assertEqual(header.model, self.w.model)
            self.assertGreater(header.voltage, 2700)
            self.assertLess(header.voltage, 3500)

            self.w.send_sleep()
            self.w.wait_off()

        finally:
            #self.w.manual_turn_off()
            pass  # ВРУЧНУЮ НАЖИМАЕМ


if __name__ == '__main__':
    unittest.main()
