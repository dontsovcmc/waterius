# -*- coding: utf-8 -*-
__author__ = 'dontsov'

from logger import log
from storage import db
from datetime import datetime, timedelta
from bot import RED, BLUE


def send_message(bot, device_id, chat_id, voltage):
    try:
        v1, v2 = db.get_current_value(device_id)
        text = u'Счетчик №{0}, V={1:.2f}\n'.format(device_id, voltage/1000.0)
        text += RED + u'{0:.3f} '.format(v1) + BLUE + u'{0:.3f}'.format(v2)
        if bot:
            bot.send_message(chat_id=chat_id, text=text, disable_notification=True, timeout=5000)
            bot.send_message(chat_id=chat_id, text=db.sms_text(device_id), disable_notification=True, timeout=5000)

    except Exception, err:
        log.error('send_message id={0}, chat={1}: {2}'.format(device_id, chat_id, err))


def send_message_job(bot, job):
    try:
        log.info('send message job: start')

        day = datetime.now().day

        for device_id in db.all_devices():
            voltage = db.get_voltage(device_id)
            prev = db.get_connect_time(device_id)

            for chat_id in db.get_chats(device_id):

                send_day = db.get_send_day(chat_id)
                if day == send_day:
                    send_message(bot, device_id, chat_id, voltage)

                if prev and datetime.now() > prev + timedelta(days=7):
                    try:
                        if bot:
                            bot.send_message(chat_id=chat_id, text=u'Счетчик №{0}: нет связи более 7 дней'.format(device_id),
                                             disable_notification=True, timeout=5000)

                    except Exception, err:
                        log.error('sending id={0}, chat={1}: {2}'.format(device_id, chat_id, err))


    except Exception, err:
        log.error('send message job: {}'.format(err))

