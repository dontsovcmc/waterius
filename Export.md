# Отправка данных по HTTP(S) на сервер

## Настройка Ватериуса

В веб интерфейсе Вотериуса заполните: waterius_host
Вид: 
```http[s]://host[:port][/path]```, [] - необязательные части

## Параметры HTTP запроса
```
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
	"imp0": 0,           //Импульсов, канал 0
	"imp1": 0,           //Импульсов, канал 1
	"version": 5,        //версия прошивки attiny
	"voltage": 2.979,    //напряжение питания, В
	"version_esp": "0.5",//прошивка ESP
	"key": "SECRET",    //ключ для waterius.ru
	"resets": 1         //количество перезагрузок 
	"email": ""         //поле электронный адрес нужно для waterius.ru
	"voltage_low": true/false //низкий заряд батарейки
	"voltage_diff": 0.020 //величина просадки питания (чтобы определить низкий заряд)
} 
```

# Поддержка HTTPS 

Начиная с версии 6.0 добавлена библиотека BearSSL с поддержкой TLS 1.2 шифрования.
В веб интерфейсе Ватериуса заполните Адрес сервера: с https
```https://host[:port][/path]```

Поддерживается проверка сертификата сервера при помощи сертификата удостоверяющего центра. 
В прошивку добавлен сертификат [Let's encrypt](https://letsencrypt.org/certificates/), т.е. Вотериус будет доверять вашему сайту, если ему выдал сертификат Let's encrypt. Инструкция по созданию сертификатов в [Python скрипте]. У сертификатов есть срок действия.

## Сервер

Вот [Python скрипт](https://github.com/dontsovcmc/waterius/blob/master/Server/server.py) Flask сервера, получающего от Вотериуса данные. HTTPS и HTTP версия, смотрите аргументы запуска.

Только HTTP сервер выглядит ещё проще:
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

if __name__ == "__main__":
	app.run(host='192.168.1.2', port=8000)
```

### Запуск
[Flask guide](http://flask.pocoo.org/docs/1.0/quickstart/)

```
python server.py
```

### Проверка
```
curl -X POST -H "Content-Type: application/json" -d '{"ch0": 1.1, "ch1": 2.0}' http://192.168.1.2:8000/data
```
Без заголовка application/json Flask не вернет JSON
