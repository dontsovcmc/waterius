# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import struct
from logger import log
from device import Data

def parse_header_2(data):
    d = Data()
    d.bytes, \
    d.version, \
    d.message_type, \
    d.wake, \
    d.voltage, \
    d.service, \
    d.reserved, \
    d.device_id, \
    d.device_pwd = struct.unpack('HBBHHBBHH', data[0:14])

    log.info("device_id = %d, MCUSR = %02x, V=%.2f" % (d.device_id, d.service, d.voltage))

    for k in xrange(14, len(data), 6):
        value1, value2, timestamp = struct.unpack('HHH', data[k:k+6])
        d.values.append((value1, value2, timestamp))
        d.timestamps.append(timestamp)

    return d
