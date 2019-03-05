# -*- coding: utf-8 -*-
from __future__ import print_function  # для единого кода Python2, Python3

import argparse
from werkzeug import serving
import ssl
from flask import Flask, request

app = Flask(__name__)


"""
Настроим защищенную передачу данных от Вотериуса. 
Создадим Центр сертификации и сгенерируем ключ/сертификат для сервера.
В Вотериус запишем сертификат Центра сертификации. Он подтвердит, что сервер тот, за кого себя выдает.

openssl genrsa -out ca_key.pem 2048
openssl req -x509 -new -nodes -key ca_key.pem -days 4096 -out ca_cer.pem
openssl genrsa -out server_key.pem 2048

# Замените 192.168.1.10 на свой домен или IP адрес сервера (!). Именно его и подтверждает сертификат.
openssl req -out server_req.csr -key server_key.pem -new -subj '/CN=192.168.1.10/C=RU/ST=Moscow/L=Moscow/O=Waterius LLC/OU=Waterius community/emailAddress=your@email.ru'

openssl x509 -req -in server_req.csr -out server_cer.pem -sha256 -CAcreateserial -days 4000 -CA ca_cer.pem -CAkey ca_key.pem


ca_key.pem - ключ вашего Центра сертификации. Храним в сейфе.
ca_cer.pem - публичный X.509 сертификат Центра сертификации для генерации ключей сервера, служит и для проверки публичного ключа сервера.
server_key.pem - приватный ключ сервера. Только на сервере.
server_cer.pem - публичный ключ сервера. Может быть передан клиентам.

В память Вотериуса записываем ca_cer.pem . Теперь Вотериус может по передавать данные зашифровано, убеждаясь, что сервер настоящий.
Сертификат имеет срок годности и требует обновления.
"""


@app.route('/', methods=['POST'])
def root():
    """
    Print Waterius values
    """
    try:
        j = request.get_json()
        print(j['ch0'])
        print(j['ch1'])
    except Exception as err:
        return "{}".format(err), 400
    return 'OK'


@app.route('/ping', methods=['GET'])
def ping():
    """
    Check server by sending GET request by Curl:

    curl https://192.168.1.1:5000/ping --cacert ./certs/ca_cer.pem
    """
    return 'pong'


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Waterius TLS server example')
    parser.add_argument('--host', help='Ip address')
    parser.add_argument('--port', type=int, default=443, help='Port')  # in Mac, Linux use sudo for port < 1000
    parser.add_argument('--cacert', default='certs/ca_cer.pem', help='CA certificate file')
    parser.add_argument('--key', default='certs/server_key.pem', help='Private key')
    parser.add_argument('--cert', default='certs/server_cer.pem', help='Certificate file')
    parser.add_argument('--http', action='store_true', help='Use HTTP protocol')
    args = parser.parse_args()

    print(args)

    if not args.http:
        context = ssl.SSLContext(ssl.PROTOCOL_TLSv1_2)
        #context.verify_mode = ssl.CERT_REQUIRED
        context.load_verify_locations(args.cacert)
        context.load_cert_chain(args.cert, args.key)
    else:
        context = None

    serving.run_simple(args.host, args.port, app, ssl_context=context)
