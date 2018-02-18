# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import sys, traceback

def exception_info():
    traceback_template = '''Traceback (most recent call last):
  File "%(filename)s", line %(lineno)s, in %(name)s
%(type)s: %(message)s\n'''

    exc_type, exc_value, exc_traceback = sys.exc_info()
    traceback_details = {
         'filename': exc_traceback.tb_frame.f_code.co_filename,
         'lineno': exc_traceback.tb_lineno,
         'name': exc_traceback.tb_frame.f_code.co_name,
         'type': exc_type.__name__,
         'message': exc_value.message}
    del(exc_type, exc_value, exc_traceback)
    return traceback.format_exc() + '\n' + traceback_template % traceback_details
