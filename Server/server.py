# -*- coding: utf-8 -*-
from __future__ import print_function  # для единого кода Python2, Python3

from flask import Flask, request, abort
import json

app = Flask(__name__)

@app.route('/data', methods=['POST'])
def data():
    try:
        j = request.get_json()
        print(j['ch0'])
        print(j['ch1'])
    except Exception as err:
        return "{}".format(err), 400
    return 'OK'