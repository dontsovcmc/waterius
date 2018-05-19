# -*- coding: utf-8 -*-
__author__ = 'dontsov'

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
        self.service = 0

        self.values = []      # (value1, value2)
        self.timestamps = []  # datetime