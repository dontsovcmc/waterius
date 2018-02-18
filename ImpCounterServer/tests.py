__author__ = 'dontsov'
import unittest
from parser import Parser

def parse(s):
    packet = s.split(' ')
    return [int(c, 16) for c in packet]

class TestPacket(unittest.TestCase):

    def test_1(self):
        d = [16, 0, 0, 0, 60, 0, 0, 0, 30, 0, 0, 0, 221, 12, 0, 0, 4, 1, 2, 58, 7, 178, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        d = [chr(i) for i in d]
        d = ''.join(d)

        p = Parser(None)
        p.handle_data(d)
        self.assertEqual(p.data.bytes, 16)
        self.assertEqual(p.data.wake, 60)
        self.assertEqual(p.data.period, 30)
        self.assertEqual(p.data.voltage, 3293)
        self.assertEqual(p.data.bytes_per_measure, 4)
        self.assertEqual(p.data.version, 1)
        self.assertEqual(p.data.device_id, 111111)
        self.assertEqual(p.data.sensors, 2)
        self.assertEqual(len(p.data.values), 2)

#0c 00 02 00 03 00 06 01 03 00 00 dd 0c 00 00 00 00 dd 0c 00 00
#ERROR integer division or modulo by zero