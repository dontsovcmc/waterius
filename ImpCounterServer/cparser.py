# -*- coding: utf-8 -*-
__author__ = 'dontsov'

from storage import db
from logger import log
import struct
from bot import RED, BLUE
from datetime import datetime, timedelta

def data2log(data):
    data = [format(ord(i), '02x') for i in data]
    return ' '.join(data)


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
                log.info("handle_data (%d): %s" % (len(data), data2log(data)))

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
            log.error("Handle error: %s, data=%s" % (str(err), data2log(data)))


def parse_type_1(data, d, bot):

    try:
        for k in xrange(14, len(data), 4):
            value1, value2 = struct.unpack('HH', data[k:k+4])
            if value1 < 65535 and value2 < 65535:  #проблемы с i2c возможно изза частоты 3%
                d.values.append((value1, value2))

        chat_list = db.get_chats(d.device_id)
        factor = db.get_factor(d.device_id)

        next_connect_str = db.get_next_connect_str(d.device_id)
        db.set_connect_time(d.device_id, datetime.utcnow())

        for chat_id in chat_list:
            if d.values:
                # Проверим, что счетчик не перезапустился
                imp1, imp2 = d.values[-1]
                prev_imp1, prev_imp2 = db.get_impulses(d.device_id)
                v1, v2 = db.get_current_value(d.device_id)
                if imp1 < prev_imp1: #либо ресет, либо 65535
                    bot.send_message(chat_id=chat_id, text=u"Переполнение счетчика ГВС. Введите тек.значение.")
                    db.set_start_value1(d.device_id, v1)
                    log.warning(u"Переполнение ГВС: было %d имп., стало %d. Перезаписана точка старта." % (prev_imp1, imp1))
                if imp2 < prev_imp2:
                    bot.send_message(chat_id=chat_id, text=u"Переполнение счетчика ХВС. Проверьте тек.значение.")
                    db.set_start_value2(d.device_id, v2)
                    log.warning(u"Переполнение ХВС: было %d имп., стало %d. Перезаписана точка старта." % (prev_imp2, imp2))

                # Пришле пользователю сообщение с показаниями
                db.set_impulses(d.device_id, imp1, imp2)
                v1, v2 = db.get_current_value(d.device_id)
                text = u'Счетчик №{0}, V={1:.2f}\nСлед.связь: {2}\n'.format(d.device_id, d.voltage/1000.0, next_connect_str)
                text += RED + u'{0:.3f} '.format(v1) + BLUE + u'{0:.3f}'.format(v2)
                if bot:
                    bot.send_message(chat_id=chat_id, text=text)

                if bot:
                    bot.send_message(chat_id=chat_id, text=db.sms_text(d.device_id))

                if not chat_list:
                    log.warn('Нет получателя для устройства #' + str(d.device_id))
            else:
                if bot:
                    bot.send_message(chat_id=chat_id, text="bad message: %s" % data2log(data))
                log.error("no values")

    except Exception, err:
        log.error("parser1 error: %s, data=%s" % (str(err), data2log(data)))
