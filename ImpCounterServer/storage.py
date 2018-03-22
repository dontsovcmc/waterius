# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import shelve
from random import randint


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
        factor = self.get(device_id, 'factor')
        return int(factor) if factor else 10

    def set_factor(self, device_id, factor):
        self.set(device_id, 'factor', factor)

    def set_select_id(self, chat_id, id):
        self.set(chat_id, 'selected', id)

    def selected_id(self, chat_id):
        return self.get(chat_id, 'selected')

    def generate_id(self):
        all = self.get('', 'ids', [])
        if len(all) == 65534:
            raise Exception('DB full')

        id = randint(1, 65535)
        while id in all:
            id = randint(1, 65535)

        all.append(id)
        self.set('', 'ids', all)
        pwd = randint(1000, 9999)

        self.set('pwd', str(id), str(pwd))
        return id, pwd

    def get_pwd(self, counter_id):
        return self.get('pwd', str(counter_id))

    def get_chat_ids(self, chat_id):
        return self.get(chat_id, 'ids', [])

    def add_id_to_chat(self, chat_id, id):
        self.add_to_list(chat_id, 'ids', id)

    def remove_id(self, chat_id, id):
        self.remove_from_list(chat_id, 'ids', id)

    def check_pwd(self, id, pwd):
        return self.get('pwd', str(id)) == str(pwd)

db = Shelve()
db.open()
