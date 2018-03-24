# -*- coding: utf-8 -*-

__author__ = 'dontsov'
import unittest
from parser import Parser
from storage import db

def parse(s):
    packet = s.split(' ')
    return [int(c, 16) for c in packet]


class TestServer(unittest.TestCase):

    def test_server(self):

        ip = "46.101.164.167"
        data = '0400013fa0053c005e0b338baf1e00000000'


class TestPacket(unittest.TestCase):


    def test_2(self):
        '''
        04 00 bytes
        01 3f version
        02 00 period
        01 00 measure period
        66 0b voltage
        33 8b id
        af 1e pwd
        00 00 00 00 data
        '''
        d = '04 00 01 3f 02 00 01 00 66 0b 33 8b af 1e 01 00 02 00'
        d = d.split(' ')
        d = [chr(int(i, 16)) for i in d]
        d = ''.join(d)

        db.add_id_to_db(35635, 7855)  # код счетчика с паролем из сообщения

        p = Parser(None)
        p.handle_data(d)
        self.assertEqual(p.data.bytes, 4)
        self.assertEqual(p.data.version, 1)
        self.assertEqual(p.data.wake, 2)
        self.assertEqual(p.data.period, 1)
        self.assertEqual(p.data.voltage, 2918)
        self.assertEqual(p.data.device_id, 35635)
        self.assertEqual(p.data.device_pwd, 7855)
        self.assertEqual(len(p.data.values), 1)
        self.assertEqual(p.data.values[0], (1, 2))


    def test_3(self):
        d = '6000013fa0053c00660b338baf1e000001000000020000000200000002000000020000000200000002000800110012001200120012001200120012001200120012ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff'
        d = [d[i:i+2] for i in range(0, len(d), 2)]
        d = [chr(int(i, 16)) for i in d]
        d = ''.join(d)

        db.add_id_to_db(35635, 7855)  # код счетчика с паролем из сообщения

        p = Parser(None)
        p.handle_data(d)
        self.assertEqual(p.data.bytes, 96)
        self.assertEqual(p.data.version, 1)
        self.assertEqual(p.data.wake, 1440)
        self.assertEqual(p.data.period, 60)
        self.assertEqual(p.data.voltage, 2918)
        self.assertEqual(p.data.device_id, 35635)
        self.assertEqual(p.data.device_pwd, 7855)
        self.assertEqual(len(p.data.values), 12)
#0c 00 02 00 03 00 06 01 03 00 00 dd 0c 00 00 00 00 dd 0c 00 00
#ERROR integer division or modulo by zero

