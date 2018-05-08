# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import socket
from tests import log2data

if __name__ == '__main__':
    '''
    04 00 bytes
    01 3f version
    02 00 period
    01 00 measure period
    66 0b voltage
    00 00 service, dummy
    33 8b id
    af 1e pwd
    00 00 00 00 data
    '''

    id = 10552
    pwd = 4449

    #d =         '04 00 01 3f 02 00 01 00 66 0b 38 29 61 11 00 00 06 00'
    d = log2data('ff ff ff 02 ff ff ff ff ff ff ff 3f eb 2c 43 0a')

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    host = "192.168.1.42"
    port = 5001
    s.connect((host, port))
    s.send(d)

    print "data send"