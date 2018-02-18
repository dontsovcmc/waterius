# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import shelve


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

    #

    def add_device(self, device_id, chat_id):

        if device_id in self.get_devices(chat_id):
            return
        device_list = self.get(chat_id, 'device_list')
        if not device_list:
            device_list = device_id
        else:
            device_list += ';' + device_id
        db.set(chat_id, 'device_list', device_list)

        chat_list = self.get(device_id, 'chat_list')
        if not chat_list:
            chat_list = str(chat_id)
        else:
            chat_list += ';' + str(chat_id)
        db.set(device_id, 'chat_list', chat_list)

    def remove_device(self, device_id, chat_id):
        device_list = self.get(chat_id, 'device_list')
        if not device_list:
            return
        else:
            l = device_list.split(';')
            l.remove(device_id)
            db.set(chat_id, 'device_list', ';'.join(l))

        chat_list = self.get(device_id, 'chat_list')
        if not chat_list:
            return
        else:
            db.set(chat_id, 'chat_list', '')

    def get_devices(self, chat_id):
        device_list = self.get(chat_id, 'device_list')
        if device_list:
            return device_list.split(';')
        return []

    def get_chats(self, device_id):
        chat_list = self.get(device_id, 'chat_list')
        if chat_list:
            return chat_list.split(';')
        return []

db = Shelve()
db.open()
