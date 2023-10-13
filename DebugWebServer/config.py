import os
from log import log

ROOT_PATH = os.path.normpath(os.path.join(os.path.abspath(__file__),
                                          os.pardir, os.pardir, 'ESP8266', 'data'))
if not os.path.exists(ROOT_PATH):
    raise Exception(f'Не найдена папка с файлами: {ROOT_PATH}')
else:
    log.info(f'root path: {ROOT_PATH}')
