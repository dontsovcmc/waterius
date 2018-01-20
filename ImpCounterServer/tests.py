__author__ = 'dontsov'
import unittest
from sensorParser import deviceID1Parse


class FakeDB:
    def writeData(self, obj, title, value, timestamp):
        pass


class FakeLogging:
    def info(self, info):
        pass


db = FakeDB()
logging = FakeLogging()

def parse_log(s):
    packet = s.split(' ')
    return [int(c, 16) for c in packet]

class TestPacket(unittest.TestCase):

    def test_1(self):
        p = parse_log("0c 00 05 00 03 00 06 01 03 00 00 dd 0c 02 00 00 00 ff ff ff ff")
        devID1parse = deviceID1Parse(logging, db)
        devID1parse.parsePacket(p)
        self.assertEqual(devID1parse.bytesPerMeasurement, 6)
        self.assertEqual(devID1parse.bytesReady, 12)
        self.assertEqual(devID1parse.deviceID, 1)
        self.assertEqual(devID1parse.expectedWakeupTime, 5)
        self.assertEqual(devID1parse.measurementsEvery, 3)
        self.assertEqual(devID1parse.sensorCount, 3)

    def test_3(self):
        p = parse_log("30 00 05 00 03 00 06 01 03 00 00 dd 0c 02 00 00 00 dd 0c 02 00 00 00 dd 0c 02 00 00 00 dd 0c 02 00 00 00 dd 0c 02 00 00 00 dd 0c 02 00 00 00 dd 0c 02 00 00 00 dd 0c 02 00")
        devID1parse = deviceID1Parse(logging, db)
        devID1parse.parsePacket(p)

#0c 00 02 00 03 00 06 01 03 00 00 dd 0c 00 00 00 00 dd 0c 00 00
#ERROR integer division or modulo by zero