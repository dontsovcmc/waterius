# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import struct
from logger import log
from device import Data

def parse_header_1(data):
    d = Data()
    d.bytes, \
    d.version, \
    d.message_type, \
    d.wake, \
    d.period, \
    d.voltage, \
    d.service, \
    d.dummy, \
    d.device_id, \
    d.device_pwd = struct.unpack('HBBHHHBBHH', data[0:16])

    log.info("device_id = %d, MCUSR = %02x, V=%.2f" % (d.device_id, d.service, d.voltage))

    for k in xrange(16, len(data), 4):
        value1, value2 = struct.unpack('HH', data[k:k+4])
        d.values.append((value1, value2, None))

    return d
