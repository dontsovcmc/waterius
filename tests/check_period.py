# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import serial
from datetime import datetime

# Скрипт печатает лог на экране с текущим временем.
# Цель: измерить разницу в 30 минутах при разном кол-ве опроса входов за 1 секунду
# Ответ: можно увеличить опрос до 8 раз в минуту.
# 30 мин = 32 мин из-за отсутствия калибровки у внутреннего резонатора Attiny85
# 
# Результаты в комментариях

def logger():
    with serial.Serial('/dev/tty.usbserial', 115200, timeout=1) as ser:
        buf = ''
        while True:
            s = ser.read(1)        # read up to ten bytes (timeout)
            if s:
                if s == '\n':
                    print str(datetime.now()) + ': ' + buf
                    buf = ''
                else:
                    buf += s


if __name__ == "__main__":
    logger()

    #500ms опрос входов
    t1 = datetime.strptime('2018-09-21 20:13:54.738461', '%Y-%m-%d %H:%M:%S.%f')
    t2 = datetime.strptime('2018-09-21 20:46:25.311879', '%Y-%m-%d %H:%M:%S.%f')
    delta500 = t2-t1  # = 32:31.108 

    #128мс опрос входов
    t1 = datetime.strptime('2018-09-21 22:00:10.687833', '%Y-%m-%d %H:%M:%S.%f')
    t2 = datetime.strptime('2018-09-21 22:32:41.844010', '%Y-%m-%d %H:%M:%S.%f')
    delta128 = t2-t1  # = 32:31:157

    #разница кол-ва запросов
    # 480 * 30 = 14400 опросов
    # 120 раз * 30 мин = 3600 опросов в минуту
    requests = 480 * 30 - 120 * 30
    delta = (delta128 - delta500).microseconds # 582759
    print delta  

    req = delta / requests  # 53 microsec

    print '1 request={} microsec'.format(req)
    print '1 day 250ms={} sec'.format(240 * 24 * 60 * req / 1000000.0) # 18.3168 sec
    print '1 day 128ms={} sec'.format(480 * 24 * 60 * req / 1000000.0) # 36.6336 sec

