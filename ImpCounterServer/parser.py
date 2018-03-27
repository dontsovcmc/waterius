# -*- coding: utf-8 -*-
__author__ = 'dontsov'

from storage import db
from logger import log
import struct
from telegram import InlineKeyboardButton, InlineKeyboardMarkup

class Data(object):
    def __init__(self):
        self.bytes = 0
        self.wake = 0
        self.period = 0
        self.voltage = 0
        self.bytes_per_measure = 0
        self.version = 0
        self.device_id = 0
        self.device_pwd = 0
        self.sensors = 0
        self.values = []

'''
struct SlaveStats {
	uint32_t bytesReady;
	uint32_t masterWakeEvery;
	uint32_t measurementEvery;
	uint32_t vcc;
	uint8_t bytesPerMeasurement;
	uint8_t version;
	uint8_t numberOfSensors;
	uint8_t dummy;
}; //should be *16bit https://github.com/esp8266/Arduino/issues/1825
'''


class Parser(object):
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

            log.info("data=%s" % ''.join( [format(ord(i), '02x') for i in data]))
            d = Data()
            d.bytes, d.version, d.dymmy, d.wake, d.period, d.voltage = struct.unpack('HBBHHH', data[0:10])
            d.device_id, d.device_pwd = struct.unpack('HH', data[10:14])

            log.info("device_id = %d" % d.device_id)

            if d.version == 1:
                if d.device_id == 12345 or db.check_pwd(d.device_id, d.device_pwd):
                    parse_type_1(data, d, self.bot)
                else:
                    log.error("Incorrect password")

            self.data = d
        except Exception, err:
            data = [format(ord(i), '02x') for i in data]
            log.error("Handle error: %s, data=%s" % (str(err), ' '.join(data)))


def parse_type_1(data, d, bot):

    try:
        value1_ = 0
        value2_ = 0
        for k in xrange(14, len(data), 4):
            value1, value2 = struct.unpack('HH', data[k:k+4])

            #проблемы с i2c возможно изза частоты 3%, иногда приходят FF
            if abs(value1 - value1_) < 1000 and abs(value2 - value2_) < 1000:
                d.values.append((value1, value2))
                value1_ = value1
                value2_ = value2
            else:
                log.error("incorrect data ({}): {}, {}".format(d.device_id, value1, value2))

        chat_list = db.get_chats(unicode(d.device_id))
        factor = db.get_factor(unicode(d.device_id))
        for chat_id in chat_list:
            if d.values:

                imp1, imp2 = d.values[-1]
                db.set_impulses(d.device_id, imp1, imp2)

                v1, v2 = db.get_current_value(d.device_id)
                imp1, imp2 = db.get_impulses(d.device_id)

                text = 'Счетчик №{0}, V={1:.2f}\n'.format(d.device_id, d.voltage/1000.0)
                text += 'ХВС: {0:.1f}м3 [{1}] ГВС: {2:.1f}м3 [{3}]'.format(v1, imp1, v2, imp2)
                if bot:
                    bot.send_message(chat_id=chat_id,
                                     text=text)
                if bot:
                    bot.send_message(chat_id=chat_id,
                                     text=db.sms_text(d.device_id),
                                     reply_markup=InlineKeyboardMarkup([[InlineKeyboardButton(u'Меню', callback_data=u'Меню')]]))

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
