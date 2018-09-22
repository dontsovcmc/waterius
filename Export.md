# HTTP сервер для получения данных с Вотериуса

## Настройка Вотериуса

В веб интерфейсе Вотериуса заполните: hostname_json
Вид: http://host:port/path
Без port будет 80
Таймаут ответа: 5 секунд

## Параметры HTTP запроса
POST
Content-Type: application/json
Connection: close

Тело:
{
	"delta0": 0,        //разница с предыдущими показаниями, вход 0, л
	"delta1": 0,        //разница с предыдущими показаниями, вход 1, л
	"good": 1,          //1=от Attiny85 получены все данные
	"boot": 0,          //Причина перезагрузки 2 - пин ресет, 3 - сброс питания  
	"ch0": 0,           //показания вход 0 в кубометрах
	"ch1": 0,           //показания вход 1 в кубометрах
	"version": 4,       //показания вход 1 в кубометрах 
	"voltage": 2.979,   //напряжение питания, В
	"version_esp": "0.5", //прошивка ESP
	"key": "SECRET",    //ключ 
	"resets": 1         //количество перезагрузок 
} 

## Пример сервера

Вот [Python скрипт](https://github.com/dontsovcmc/waterius/blob/master/Server/Server.py) Flask сервера, получающего от Вотериуса данные.

```
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
```

### Запуск
WIN cmd: set FLASK_APP=server.py
WIN powershell: $env:FLASK_APP = "server.py"
UNIX: export FLASK_APP=server.py

```
python -m flask run --host=x.x.x.x --port=x
```
[Flask guide](http://flask.pocoo.org/docs/1.0/quickstart/)

### Проверка
```
curl -X POST -H "Content-Type: application/json" -d '{"ch0": 1.1, "ch1": 2.0}' http://x.x.x.x:x/data
```
Без заголовка application/json Flask не вернет JSON
