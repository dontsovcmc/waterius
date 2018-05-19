# -*- coding: utf-8 -*-

__author__ = 'dontsov'
import unittest
from cparser import CounterParser
from storage import db
import socket


def log2data(s):
    d = s.split(' ') if ' ' in s else [s[i:i + 2] for i in range(0, len(s), 2)]
    d = [chr(int(i, 16)) for i in d]
    return ''.join(d)


class TestDB(unittest.TestCase):

    def test_value(self):

        try:
            id = 1
            imp1, imp2 = db.get_impulses(id)
            assert not imp1 and not imp2

            v1, v2 = db.get_current_value(id)
            assert not v1 and not v2

            db.set_impulses(id, 5, 6)

            imp1, imp2 = db.get_impulses(id)
            assert imp1 == 5 and imp2 == 6

            factor = db.get_factor(id)
            assert factor == 1

            v1, v2 = db.get_current_value(id)
            assert v1 == 5*factor/1000.0 and v2 == 6*factor/1000.0

            db.set_start_value1(id, 100)
            db.set_start_value2(id, 200)

            v1, v2 = db.get_current_value(id)
            assert v1 == 100 and v2 == 200

            db.set_impulses(id, 10, 11)

            v1, v2 = db.get_current_value(id)
            assert v1 == (10-5)*factor/1000.0 + 100
            assert v2 == (11-6)*factor/1000.0 + 200
        finally:
            db.set_impulses(id, 0, 0)
            db.set_start_value1(id, 0)
            db.set_start_value2(id, 0)


class TestServer(unittest.TestCase):

    def test_server(self):

        d = log2data('0400013fa0053c005e0b338baf1e00000000')

        '''
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        host = ""
        port = 5001
        s.connect((host, port))
        s.send(d)

        print "data send"
        '''


class TestPacket(unittest.TestCase):

    def test_1_parse_id2(self):
        d = log2data('0c 00 02 01 01 00 b3 0b 00 00 31 0f f3 1a 03 00 00 00 01 00 03 00 00 00 0a 00')
        db.add_id_to_db(3889, 6899)  # код счетчика с паролем из сообщения

        p = CounterParser(None)
        p.handle_data(d)
        self.assertEqual(p.header.bytes, 0x0C)
        self.assertEqual(p.header.version, 2)
        self.assertEqual(p.header.wake, 1)
        self.assertEqual(p.header.voltage, 2995)
        self.assertEqual(p.header.device_id, 3889)
        self.assertEqual(p.header.device_pwd, 6899)
        self.assertEqual(len(p.header.values), 2)
        self.assertEqual(len(p.header.timestamps), 2)
        self.assertEqual(p.header.values[0], (3, 0))
        self.assertEqual(p.header.values[0], (3, 0))

    def test_2_parse_id1(self):
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
        d = log2data('04 00 01 3f 02 00 01 00 66 0b 00 00 33 8b af 1e 01 00 02 00')
        db.add_id_to_db(35635, 7855)  # код счетчика с паролем из сообщения

        p = CounterParser(None)
        p.handle_data(d)
        self.assertEqual(p.header.bytes, 4)
        self.assertEqual(p.header.version, 1)
        self.assertEqual(p.header.wake, 2)
        self.assertEqual(p.header.period, 1)
        self.assertEqual(p.header.voltage, 2918)
        self.assertEqual(p.header.device_id, 35635)
        self.assertEqual(p.header.device_pwd, 7855)
        self.assertEqual(len(p.header.values), 1)
        self.assertEqual(p.header.values[0], (1, 2))

    def test_3_parse_id1(self):
        d = log2data('6000013fa0053c00660b0000338baf1e0000010000000200080012ffffffffff')

        db.add_id_to_db(35635, 7855)  # код счетчика с паролем из сообщения

        p = CounterParser(None)
        p.handle_data(d)
        self.assertEqual(p.header.bytes, 96)
        self.assertEqual(p.header.version, 1)
        self.assertEqual(p.header.wake, 1440)
        self.assertEqual(p.header.period, 60)
        self.assertEqual(p.header.voltage, 2918)
        self.assertEqual(p.header.device_id, 35635)
        self.assertEqual(p.header.device_pwd, 7855)
        self.assertEqual(len(p.header.values), 4)

    def test_4_parse_id1(self):
        d = log2data('60 00 01 3f a0 05 3c 00 75 0b 00 00 33 8b af 1e 52 00 64 00 52 00 64 00 52 00 64 00 52 00 64 00 52 00 32 00 54 00 66 00 55 00 69 00 55 00 69 00 55 00 69 00 55 00 69 00 55 ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff')
        db.add_id_to_db(35635, 7855)  # код счетчика с паролем из сообщения
        p = CounterParser(None)
        p.handle_data(d)
        self.assertEqual(p.header.bytes, 96)
        self.assertEqual(p.header.version, 1)
        self.assertEqual(p.header.wake, 1440)
        self.assertEqual(p.header.period, 60)
        self.assertEqual(p.header.voltage, 2933)
        self.assertEqual(p.header.device_id, 35635)
        self.assertEqual(p.header.device_pwd, 7855)
        self.assertEqual(len(p.header.values), 24) #убрал проверку на FF

    def test_5(self):
        #04 00 01 01 3c 00 05 00 16 0c 03 3f eb 2c 43 0a 01 00 01 00

        pass

