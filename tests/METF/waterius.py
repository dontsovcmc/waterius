# -*- coding: utf-8 -*-
from __future__ import unicode_literals
import os
import sys
import time
import argparse

from metf_python_client import METFClient, log, LOW, HIGH, INPUT, OUTPUT, INPUT_PULLUP
#from ESPTestFramework.boards.nodemcu import D0, D1, D2, D3, D4, D5
from metf_python_client.boards.wemos import D0, D1, D2, D3, D4, D5
from metf_python_client.utils import DataStruct


WATERIUS_CLASSIC = 0
WATERIUS_4C2W = 1


def dallas_crc8(buf, size):
    # https://crccalc.com/
    # https://gist.github.com/brimston3/83cdeda8f7d2cf55717b83f0d32f9b5e
    # https://www.onlinegdb.com/online_c++_compiler
    # Dallas CRC x8+x5+x4+1

    crc = 0
    for a in range(0, size):
        i = (ord(buf[a]) ^ crc) & 0xFF
        crc = 0
        if i & 0x01:
            crc ^= 0x5e
        if i & 0x02:
            crc ^= 0xbc
        if i & 0x04:
            crc ^= 0x61
        if i & 0x08:
            crc ^= 0xc2
        if i & 0x10:
            crc ^= 0x9d
        if i & 0x20:
            crc ^= 0x23
        if i & 0x40:
            crc ^= 0x46
        if i & 0x80:
            crc ^= 0x8c

    return crc


#https://github.com/lammertb/libcrc/blob/600316a01924fd5bb9d47d535db5b8f3987db178/src/crc8.c
sht75_crc_table = [
    0,   49,  98,  83,  196, 245, 166, 151, 185, 136, 219, 234, 125, 76,  31,  46,
    67,  114, 33,  16,  135, 182, 229, 212, 250, 203, 152, 169, 62,  15,  92,  109,
    134, 183, 228, 213, 66,  115, 32,  17,  63,  14,  93,  108, 251, 202, 153, 168,
    197, 244, 167, 150, 1,   48,  99,  82,  124, 77,  30,  47,  184, 137, 218, 235,
    61,  12,  95,  110, 249, 200, 155, 170, 132, 181, 230, 215, 64,  113, 34,  19,
    126, 79,  28,  45,  186, 139, 216, 233, 199, 246, 165, 148, 3,   50,  97,  80,
    187, 138, 217, 232, 127, 78,  29,  44,  2,   51,  96,  81,  198, 247, 164, 149,
    248, 201, 154, 171, 60,  13,  94,  111, 65,  112, 35,  18,  133, 180, 231, 214,
    122, 75,  24,  41,  190, 143, 220, 237, 195, 242, 161, 144, 7,   54,  101, 84,
    57,  8,   91,  106, 253, 204, 159, 174, 128, 177, 226, 211, 68,  117, 38,  23,
    252, 205, 158, 175, 56,  9,   90,  107, 69,  116, 39,  22,  129, 176, 227, 210,
    191, 142, 221, 236, 123, 74,  25,  40,  6,   55,  100, 85,  194, 243, 160, 145,
    71,  118, 37,  20,  131, 178, 225, 208, 254, 207, 156, 173, 58,  11,  88,  105,
    4,   53,  102, 87,  192, 241, 162, 147, 189, 140, 223, 238, 121, 72,  27,  42,
    193, 240, 163, 146, 5,   52,  103, 86,  120, 73,  26,  43,  188, 141, 222, 239,
    130, 179, 224, 209, 70,  119, 36,  21,  59,  10,  89,  104, 255, 206, 157, 172
]


def waterius12_crc(buf, size):
    crc = 0
    for i in range(0, size):
        crc = sht75_crc_table[ord(buf[i]) ^ crc]
    return crc


class Waterius(object):

    ATTINY_VER = 13

    SETUP_MODE = 1
    TRANSMIT_MODE = 2

    ADDR = 10

    def __init__(self, api):
        self.api = api

    def wait_off(self):
        log.info('ESP: wait power pin LOW')
        assert self.api.wait_digital(self.power_pin, LOW, 2.0)
        self.api.delay(20)

    def manual_turn_off(self):
        self.api.pinMode(self.power_pin, INPUT)
        if self.api.digitalRead(self.power_pin) == HIGH:
            log.info('--------------')
            log.info('ESP: manual turn off')
            self.push_button(4000)
        else:
            log.info('ESP: Already off')

    def start_i2c(self):
        self.api.i2c_begin(self.sda, self.sdl)
        self.api.i2c_setClock(100000)
        self.api.i2c_setClockStretchLimit(2000)

    def stop_i2c(self):
        self.api.i2c_flush()

    def get_mode(self):
        log.info('ESP: get mode')
        ret = self.api.i2c_ask(self.ADDR, 'M', 1)
        return ord(ret[0])

    def send_sleep(self):
        log.info('ESP: send sleep')
        ret = self.api.i2c_ask(self.ADDR, 'Z', 1)
        return ret

    def get_header(self):
        fields = [  # name, type
            ('version',   'B'),  # unsigned char
            ('service',   'B'),
            ('voltage',   'L'),
            ('resets',    'B'),
            ('model',     'B'),
            ('state0',    'B'),
            ('state1',    'B'),
            ('impulses0', 'L'),
            ('impulses1', 'L'),
            ('adc0',      'H'),  # unsigned short
            ('adc1',      'H'),
            ('crc',       'B'),
            ('reserved1', 'B')
        ]

        header_len = DataStruct.calcsize(fields)
        crc_shift = DataStruct.calcsize(fields, 'crc')

        log.info('ESP: get header')

        ret = self.api.i2c_ask(self.ADDR, 'B', header_len)

        header = DataStruct(fields, ret)

        if header.version < 13:
            if header.crc == waterius12_crc(ret, crc_shift):
                return header
        else:
            if header.crc == dallas_crc8(ret, crc_shift):
                return header

        raise Exception('некорректный CRC')

    def impulse(self, pins=[], **kwargs):

        width = kwargs.get('width', 300)
        pause = kwargs.get('pause', 800)

        for pin in pins:
            self.api.pinMode(pin, OUTPUT)
            self.api.digitalWrite(pin, HIGH)
            log.info('ESP: impulse on pin {}'.format(pin))

        self.api.delay(width)

        for pin in pins:
            self.api.digitalWrite(pin, LOW)

        for pin in pins:
            self.api.pinMode(pin, INPUT)

        self.api.delay(pause)


class WateriusClassic(Waterius):

    model = WATERIUS_CLASSIC

    def __init__(self, api):
        Waterius.__init__(self, api)

        self.counter0_pin = D5
        self.counter1_pin = D0
        self.button_pin = D1
        self.power_pin = D2
        self.sda = D3
        self.sdl = D4

    def init(self):
        self.api.pinMode(self.sda, INPUT_PULLUP)
        self.api.pinMode(self.sdl, INPUT_PULLUP)
        self.api.pinMode(self.power_pin, INPUT)
        self.api.pinMode(self.counter0_pin, INPUT)
        self.api.pinMode(self.counter1_pin, INPUT)

    def push_button(self, msec):
        log.info('ESP: PUSH BUTTON for {} msec'.format(msec))
        self.api.pinMode(self.button_pin, OUTPUT)
        self.api.digitalWrite(self.button_pin, LOW)
        self.api.delay(msec)
        self.api.digitalWrite(self.button_pin, HIGH)
        self.api.pinMode(self.button_pin, INPUT)

    def wake_up(self, wait_wakeup_sec=2.0):

        if self.api.digitalRead(self.power_pin) == LOW:
            self.push_button(500)

        assert self.api.wait_digital(self.power_pin, HIGH, wait_wakeup_sec), 'ESP: wake up error'

    def wait_off(self):
        log.info('ESP: wait power pin LOW')
        assert self.api.wait_digital(self.power_pin, LOW, 2.0)
        self.api.delay(20)

    def manual_turn_off(self):
        self.api.pinMode(self.power_pin, INPUT)
        if self.api.digitalRead(self.power_pin) == HIGH:
            log.info('--------------')
            log.info('ESP: manual turn off')
            self.push_button(4000)
        else:
            log.info('ESP: Already off')

    def get_header(self):
        fields = [  # name, type
            ('version',   'B'),  # unsigned char
            ('service',   'B'),
            ('voltage',   'L'),
            ('resets',    'B'),
            ('model',     'B'),
            ('state0',    'B'),
            ('state1',    'B'),
            ('impulses0', 'L'),
            ('impulses1', 'L'),
            ('adc0',      'H'),  # unsigned short
            ('adc1',      'H'),
            ('crc',       'B'),
            ('reserved1', 'B')
        ]

        header_len = DataStruct.calcsize(fields)
        crc_shift = DataStruct.calcsize(fields, 'crc')

        log.info('ESP: get header')

        ret = self.api.i2c_ask(self.ADDR, 'B', header_len)

        header = DataStruct(fields, ret)

        for f in fields:
            log.info('{}: {}'.format(f[0], getattr(header, f[0])))

        if header.version < 12:
            return header

        if header.version < 13:
            if header.crc == waterius12_crc(ret, crc_shift):
                return header
        else:
            if header.crc == dallas_crc8(ret, crc_shift):
                return header

        raise Exception('некорректный CRC')

    def impulse(self, pins=[]):
        super(WateriusClassic, self).impulse([self.counter0_pin, self.counter1_pin])


class Waterius4C2W(Waterius):

    model = WATERIUS_4C2W

    def __init__(self, api):
        Waterius.__init__(self, api)

        self.counter0_pin = D5
        self.counter1_pin = D0
        #self.button_pin = D1
        self.power_pin = D2
        self.sda = D3
        self.sdl = D4

    def init(self):
        self.api.pinMode(self.sda, INPUT_PULLUP)
        self.api.pinMode(self.sdl, INPUT_PULLUP)
        self.api.pinMode(self.power_pin, INPUT)
        self.api.pinMode(self.counter0_pin, INPUT)
        self.api.pinMode(self.counter1_pin, INPUT)

    def wake_up(self, wait_wakeup_sec=2.0):
        log.warn("don't support wake_up")

    def wait_off(self):
        log.info('ESP: wait power pin LOW')
        assert self.api.wait_digital(self.power_pin, LOW, 2.0)
        self.api.delay(20)

    def manual_turn_off(self):
        log.warn("don't support manual_turn_off")

    def get_header(self):
        fields = [  # name, type
            ('version',   'B'),  # unsigned char
            ('service',   'B'),
            ('voltage',   'L'),
            ('resets',    'B'),
            ('model',     'B'),
            ('state0',    'B'),
            ('state1',    'B'),
            ('state2',    'B'),
            ('state3',    'B'),
            ('impulses0', 'L'),
            ('impulses1', 'L'),
            ('impulses2', 'L'),
            ('impulses3', 'L'),
            ('adc0',      'H'),  # unsigned short
            ('adc1',      'H'),
            ('adc2',      'H'),
            ('adc3',      'H'),
            ('crc',       'B'),
            ('reserved1', 'B')
        ]

        header_len = DataStruct.calcsize(fields)
        crc_shift = DataStruct.calcsize(fields, 'crc')

        log.info('ESP: get header')

        ret = self.api.i2c_ask(self.ADDR, 'B', header_len)

        header = DataStruct(fields, ret)

        for f in fields:
            log.info('{}: {}'.format(f[0], getattr(header, f[0])))

        if header.version < 13:
            if header.crc == waterius12_crc(ret, crc_shift):
                return header
        else:
            if header.crc == dallas_crc8(ret, crc_shift):
                return header

        raise Exception('некорректный CRC')

    def impulse(self, pins=[], **kwargs):
        super(Waterius4C2W, self).impulse([self.counter0_pin, self.counter1_pin], **kwargs)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Waterius test framework by METF', add_help=False)
    parser.add_argument('--help', action='store_true', help='Print help')
    parser.add_argument('-h', '--host', default='192.168.3.46', help='ESP METF host')
    parser.add_argument('--board', default='2c', help='Waterius 2c or 4c')
    parser.add_argument('--i2c', action='store_true', help='Setup i2c in ESP')
    parser.add_argument('--mode', action='store_true', help='Send mode command')
    parser.add_argument('--sleep', action='store_true', help='Send sleep command')
    parser.add_argument('--header', action='store_true', help='Send get header command')
    parser.add_argument('--wakeup', action='store_true', help='Imitation of button press')
    parser.add_argument('--impulse', type=int, default=0, help='Send impulse')
    parser.add_argument('--imitation', action='store_true', help='Imitation of waterius ESP turn on for transmitting')

    args = parser.parse_args()

    if args.help:
        print parser.format_help()
        sys.exit(1)

    api = METFClient(args.host)

    if args.board == '2c':
        w = WateriusClassic(api)
    elif args.board == '4c':
        w = Waterius4C2W(api)

    if args.i2c:
        w.start_i2c()

    if args.wakeup:
        w.wake_up()

    if args.mode:
        w.get_mode()

    if args.header:
        w.get_header()

    if args.sleep:
        w.send_sleep()
        w.wait_off()

    if args.impulse > 0:
        for i in range(0, args.impulse):
            log.info('Impulse {} of {}'.format(i, args.impulse))
            w.impulse()

    if args.imitation:
        log.info('Imitation of turn on Wi-Fi')
        w.wake_up()
        w.start_i2c()
        w.get_mode()

        t = time.time()
        while time.time() - t < 10.0:
            w.get_header()
            time.sleep(0.2)

        w.send_sleep()
        w.wait_off()
