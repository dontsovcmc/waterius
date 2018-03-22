__author__ = 'dontsov'
import unittest
from parser import Parser

def parse(s):
    packet = s.split(' ')
    return [int(c, 16) for c in packet]


class TestServer(unittest.TestCase):

    def test_server(self):

        ip = "46.101.164.167"



class TestPacket(unittest.TestCase):

    def test_1(self):

        d = '5c 00 01 3f 02 00 01 00 84 0b 39 30 39 30 20 40 00 00 00 00 00 00 00 00 00 00'
        d = d.split(' ')
        d = [chr(int(i, 16)) for i in d]
        d = ''.join(d)
        p = Parser(None)
        p.handle_data(d)
        self.assertEqual(p.data.bytes, 92)
        self.assertEqual(p.data.version, 1)
        self.assertEqual(p.data.wake, 2)
        self.assertEqual(p.data.period, 1)
        self.assertEqual(p.data.voltage, 2948)
        self.assertEqual(p.data.device_id, 12345)
        self.assertEqual(p.data.device_pwd, 12345)
        self.assertEqual(len(p.data.values), 3)

        d = '04 00 01 3f 02 00 01 00 66 0b 39 30 39 30 20 40 00 00 00 00'
        d = d.split(' ')
        d = [chr(int(i, 16)) for i in d]
        d = ''.join(d)
        p = Parser(None)
        p.handle_data(d)
        self.assertEqual(p.data.bytes, 4)
        self.assertEqual(p.data.version, 1)
        self.assertEqual(p.data.wake, 2)
        self.assertEqual(p.data.period, 1)
        self.assertEqual(p.data.voltage, 2918)
        self.assertEqual(p.data.device_id, 12345)
        self.assertEqual(p.data.device_pwd, 12345)
        self.assertEqual(len(p.data.values), 1)
#0c 00 02 00 03 00 06 01 03 00 00 dd 0c 00 00 00 00 dd 0c 00 00
#ERROR integer division or modulo by zero

