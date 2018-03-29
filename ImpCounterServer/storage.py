# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import shelve
from random import randint
from datetime import datetime, timedelta

COLD_HOT, HOT_COLD = range(2)

class Shelve(object):

    def __init__(self, title='shelve.db'):
        self.storage = None
        self.title = title

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def open(self):
        if not self.storage:
            self.storage = shelve.open(self.title)

    def close(self):
        if self.storage:
            self.storage.close()
        self.storage = None

    def set(self, chat_id, field, value):
        self.storage[str(chat_id) + field] = value
        self.storage.sync()  # !!!!

    def get(self, chat_id, field, default=None):
        try:
            return self.storage[str(chat_id) + field]
        except KeyError:
            return default

    def add_to_list(self, chat_id, field, item):
        l = self.get(str(chat_id), field, [])
        if item in l:
            return
        l.append(item)
        db.set(chat_id, field, l)

    def remove_from_list(self, chat_id, field, item):
        l = self.get(str(chat_id), field, [])
        if item in l:
            l.remove(item)
            self.set(chat_id, field, l)
    #

    def add_device(self, device_id, chat_id):
        self.add_to_list(chat_id, 'device_list', device_id)
        self.add_to_list(device_id, 'chat_list', chat_id)

    def remove_device(self, device_id, chat_id):
        self.remove_from_list(chat_id, 'device_list', device_id)
        self.remove_from_list(device_id, 'chat_list', chat_id)

    def get_devices(self, chat_id):
        return self.get(chat_id, 'device_list', [])

    def get_chats(self, device_id):
        return self.get(device_id, 'chat_list', [])

    def get_factor(self, device_id):
        return self.get(device_id, 'factor', 1)

    def sms_text(self, device_id):
        v1, v2 = self.get_current_value(device_id)
        order = self.get_order(device_id, COLD_HOT)
        if order == COLD_HOT:
            return u'вода добавить {0:.1f} {1:.1f}'.format(v1, v2)
        else:
            return u'вода добавить {0:.1f} {1:.1f}'.format(v2, v1)

    def set_factor(self, device_id, factor):
        self.set(device_id, 'factor', factor)

    def set_order(self, device_id, order):
        self.set(device_id, 'order', order)

    def get_order(self, device_id, default):
        return self.get(device_id, 'order', default)

    def set_select_id(self, chat_id, id):
        self.set(chat_id, 'selected', id)

    def selected_id(self, chat_id):
        return self.get(chat_id, 'selected')

    def generate_id(self):
        id = randint(1, 65535)
        while not self.id_exist(id):
            id = randint(1, 65535)
        pwd = randint(1000, 9999)

        self.add_id_to_db(id, pwd)
        return id, pwd

    def id_exist(self, id):
        all = self.get('', 'ids', [])
        return id in all

    def add_id_to_db(self, id, pwd):
        all = self.get('', 'ids', [])
        if len(all) == 65534:
            raise Exception('DB full')
        all.append(id)
        self.set('', 'ids', all)
        self.set('pwd', str(id), str(pwd))

    def get_pwd(self, counter_id):
        return self.get('pwd', str(counter_id))

    def get_chat_ids(self, chat_id):
        return self.get(chat_id, 'ids', [])

    def add_id_to_chat(self, chat_id, id):
        self.add_to_list(chat_id, 'ids', id)

    def remove_id(self, chat_id, id):
        self.remove_from_list(chat_id, 'ids', id)

    def get_pwd(self, id):
        return self.get('pwd', str(id), 0)

    def check_pwd(self, id, pwd):
        return self.get_pwd(id) == str(pwd)

    def set_impulses(self, id, imp1, imp2):
        '''
        :param id:
        :param imp1: штук
        :param imp2: штук
        '''
        self.set(id, 'imp1', imp1)
        self.set(id, 'imp2', imp2)

    def get_impulses(self, id):
        imp1 = self.get(id, 'imp1', 0)
        imp2 = self.get(id, 'imp2', 0)
        return imp1, imp2

    def set_start_value1(self, id, v1):
        self.set(id, 'start1', v1)
        imp1, imp2 = self.get_impulses(id)
        self.set(id, 'start_imp1', imp1)

    def set_start_value2(self, id, v2):
        self.set(id, 'start2', v2)
        imp1, imp2 = self.get_impulses(id)
        self.set(id, 'start_imp2', imp2)

    def set_connect_time(self, id, timestamp):
        self.set(id, "connect_time", timestamp)

    def get_connect_time(self, id):
        return self.get(id, "connect_time", None)

    def get_next_connect_str(self, id):
        prev = db.get_connect_time(id)
        if prev:
            next_connect = datetime.now() + timedelta(seconds=(datetime.now() - prev).total_seconds())
            return str(next_connect)
        return "см.настройки счетчика"

    def get_current_value(self, id):
        '''
        :param id: номер устройства
        :return: кубометров, кубометров
        '''
        imp1, imp2 = self.get_impulses(id)

        imp1_0 = self.get(id, 'start_imp1', 0)
        imp2_0 = self.get(id, 'start_imp2', 0)

        factor = self.get_factor(id)

        value1 = (imp1 - imp1_0)*factor/1000.0 + self.get(id, 'start1', 0)
        value2 = (imp2 - imp2_0)*factor/1000.0 + self.get(id, 'start2', 0)

        return value1, value2


db = Shelve()
db.open()
