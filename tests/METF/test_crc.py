# -*- coding: utf-8 -*-
import unittest
from metf_python_client import log, METFClient, LOW, HIGH, INPUT, OUTPUT, INPUT_PULLUP, str2hex, hex2str

from waterius import dallas_crc8, WateriusAttiny_13


ESP_HOST = '192.168.3.46'


class TestCrc8(unittest.TestCase):

    def test_crc(self):
        self.assertEqual(str2hex('EB'), chr(dallas_crc8(str2hex('3132'), 2)))
        self.assertEqual(str2hex('31'), chr(dallas_crc8(str2hex('0000145D6E0F01'), 7)))


if __name__ == '__main__':
    unittest.main()
