# -*- coding: utf-8 -*-
__author__ = 'dontsov'

from storage import db
from logger import log
import struct
from datetime import datetime, timedelta
from device1_parser import parse_header_1
from device2_parser import parse_header_2


def data2log(data):
    data = [format(ord(i), '02x') for i in data]
    return ' '.join(data)


class CounterParser(object):
    def __init__(self, bot):
        self.bot = bot
        self.header = None

    def handle_data(self, data):
        try:
            if not data:
                log.error("Data lenght=0")
                return
            else:
                log.info("handle_data (%d): %s" % (len(data), data2log(data)))

            bytes, version = struct.unpack('HB', data[0:3])

            if version == 1:

                self.header = parse_header_1(data)

                if db.check_pwd(self.header.device_id, self.header.device_pwd):
                    apply_new_data(self.header, self.bot)
                else:
                    log.error("Incorrect password")

            elif version == 2:

                self.header = parse_header_2(data)

                if db.check_pwd(self.header.device_id, self.header.device_pwd):
                    apply_new_data(self.header, self.bot)
                else:
                    log.error("Incorrect password")

        except Exception, err:
            log.error("Handle error: %s, data=%s" % (str(err), data2log(data)))


def apply_new_data(d, bot):

    chat_list = db.get_chats(d.device_id)
    #factor = db.get_factor(d.device_id)

    #next_connect = db.get_next_connect(d.device_id)
    db.set_connect_time(d.device_id, datetime.utcnow())

    if d.values:
        imp1, imp2 = d.values[-1]

        prev_imp1, prev_imp2 = db.get_impulses(d.device_id)
        v1, v2 = db.get_current_value(d.device_id)

        db.set_impulses(d.device_id, imp1, imp2)
        db.set_voltage(d.device_id, d.voltage)

        if not chat_list:
            log.error("нет чатов для устройства #" + str(d.device_id))

        for chat_id in chat_list:
            # Проверим, что счетчик не перезапустился
            if imp1 < prev_imp1: #либо ресет, либо 65535
                bot.send_message(chat_id=chat_id, text=u"Переполнение счетчика ГВС. Введите тек.значение.")
                db.set_start_value1(d.device_id, v1)
                log.warning(u"Переполнение ГВС: было %d имп., стало %d. Перезаписана точка старта." % (prev_imp1, imp1))
            if imp2 < prev_imp2:
                bot.send_message(chat_id=chat_id, text=u"Переполнение счетчика ХВС. Проверьте тек.значение.")
                db.set_start_value2(d.device_id, v2)
                log.warning(u"Переполнение ХВС: было %d имп., стало %d. Перезаписана точка старта." % (prev_imp2, imp2))

            '''
            # Сообщение с показаниями пользователю
            v1, v2 = db.get_current_value(d.device_id)
            text = u'Счетчик №{0}, V={1:.2f}\nСлед.связь: {2}\n'.format(d.device_id, d.voltage/1000.0, next_connect_str)
            text += RED + u'{0:.3f} '.format(v1) + BLUE + u'{0:.3f}'.format(v2)
            if bot:
                bot.send_message(chat_id=chat_id, text=text)

            if bot:
                bot.send_message(chat_id=chat_id, text=db.sms_text(d.device_id))

            '''
    else:
        #if bot:
        #   bot.send_message(chat_id=chat_id, text="bad message: %s" % data2log(data))
        log.error("no values")

    if not chat_list:
        log.warn('Нет получателя для устройства #' + str(d.device_id))

