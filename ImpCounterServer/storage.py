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

db = Shelve()
db.open()
