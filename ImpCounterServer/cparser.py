# -*- coding: utf-8 -*-
__author__ = 'dontsov'

from storage import db
from logger import log
import struct


class Data(object):
    def __init__(self):
        self.bytes = 0
        self.wake = 0
        self.period = 0
        self.voltage = 0
        self.bytes_per_measure = 0
        self.version = 0
        self.device_id = 0
        self.sensors = 0
        self.values = []


class CounterParser(object):
    def __init__(self, bot):
        self.bot = bot
        self.data = Data()

    def handle_data(self, data):
        try:
            if not data:
                log.error("Data lenght=0")
                return
            else:
                log.info("handle_data length: %d" % len(data))

            #data = [ord(i) for i in data]
            d = Data()
            d.bytes, d.version, d.dymmy, d.wake, d.period, d.voltage = struct.unpack('HBBHHH', data[0:10])
            d.device_id, d.device_pwd = struct.unpack('HH', data[10:14])

            log.info("device_id = %d" % d.device_id)

            if d.version == 1:
                if db.check_pwd(d.device_id, d.device_pwd):
                    parse_type_1(data, d, self.bot)
                else:
                    log.error("Incorrect password")

            self.data = d
        except Exception, err:
            data = [format(ord(i), '02x') for i in data]
            log.error("Handle error: %s, data=%s" % (str(err), ' '.join(data)))


def parse_type_1(data, d, bot):

    try:
        for k in xrange(14, len(data), 4):
            value1, value2 = struct.unpack('HH', data[k:k+4])
            if value1 < 65535 and value2 < 65535:  #проблемы с i2c возможно изза частоты 3%
                d.values.append((value1, value2))

        chat_list = db.get_chats(unicode(d.device_id))
        factor = db.get_factor(unicode(d.device_id))
        for chat_id in chat_list:
            if d.values:
                text = 'Счетчик №{0}, V={1:.2f}\n'.format(d.device_id, d.voltage/1000.0)
                text += 'ХВС: {0}л ГВС: {1}л'.format(int(d.values[-1][0]*factor), int(d.values[-1][1]*factor))
                if bot:
                    bot.send_message(chat_id=chat_id, text=text)

                text = 'вода добавить {0:.1f} {1:.1f}'.format(d.values[-1][0]*factor/1000.0, d.values[-1][1]*factor/1000.0)
                if bot:
                    bot.send_message(chat_id=chat_id, text=text)

                if not chat_list:
                    log.warn('Нет получателя для устройства #' + str(d.device_id))
            else:
                if bot:
                    text = ' '.join(format(ord(i), '02x') for i in data[14:len(data)])
                    bot.send_message(chat_id=chat_id, text="bad message: %s" % text)
                log.error("no values")

    except Exception, err:
        data = [format(ord(i), '02x') for i in data]
        log.error("parser1 error: %s, data=%s" % (str(err), ' '.join(data)))
