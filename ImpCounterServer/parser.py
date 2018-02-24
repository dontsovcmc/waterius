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
            print "parser get data lenght: %d" % len(data)

            #data = [ord(i) for i in data]
            d = Data()
            d.bytes, d.wake, d.period, d.voltage = struct.unpack('IIII', data[0:16])
            d.bytes_per_measure, d.version, d.sensors, dummy, d.device_id = struct.unpack('BBBBI', data[16:24])

            if d.version == 1:
                parse_type_1(data, d, self.bot)

            self.data = d
        except Exception, err:
            data = [format(ord(i), '02x') for i in data]
            log.error("Handle error: %s, data=%s" % (str(err), ' '.join(data)))


def parse_type_1(data, d, bot):

    try:
        for k in xrange(24, len(data), 8):
            value1, value2 = struct.unpack('II', data[k:k+8])
            if value1 < 1000000000 and value2 < 1000000000:  #проблемы с i2c
                d.values.append((value1, value2))

        if d.values:
            chat_list = db.get_chats(unicode(d.device_id))
            for chat_id in chat_list:
                text = 'Счетчик №{0}, V={1:.2f}\n'.format(d.device_id, d.voltage/1000.0)
                text += 'ХВС: {0}л ГВС: {1}л'.format(d.values[-1][0],d.values[-1][1])
                if bot:
                    bot.send_message(chat_id=chat_id, text=text)

                text = 'вода добавить {0:.1f} {1:.1f}'.format(d.values[-1][0]/1000.0, d.values[-1][1]/1000.0)
                if bot:
                    bot.send_message(chat_id=chat_id, text=text)


            if not chat_list:
                log.warn('Нет получателя для устройства #' + str(d.device_id))
        else:
            raise Exception("no values")

    except Exception, err:
        data = [format(ord(i), '02x') for i in data]
        log.error("parser1 error: %s, data=%s" % (str(err), ' '.join(data)))
