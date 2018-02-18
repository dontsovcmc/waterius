'''Using Webhook and self-signed certificate'''

# This file is an annotated example of a webhook based bot for
# telegram. It does not do anything useful, other than provide a quick
# template for whipping up a testbot. Basically, fill in the CONFIG
# section and run it.
# Dependencies (use pip to install them):
# - python-telegram-bot: https://github.com/leandrotoledo/python-telegram-bot
# - Flask              : http://flask.pocoo.org/
# Self-signed SSL certificate (make sure 'Common Name' matches your FQDN):
# $ openssl req -new -x509 -nodes -newkey rsa:1024 -keyout server.key -out server.crt -days 3650
# You can test SSL handshake running this script and trying to connect using wget:
# $ wget -O /dev/null https://$HOST:$PORT/

import os
import sys
import argparse
from signal import signal, SIGINT, SIGTERM, SIGABRT

from server import TcpServer
from parser import Parser
from telegram.ext import Updater, MessageHandler, Filters
from storage import db
from logger import log
from bot import conv_handler, error_handler, outside_handler

CERT     = 'server.crt'
CERT_KEY = 'server.key'

# CONFIG
TOKEN = os.environ['BOT_TOKEN'] if 'BOT_TOKEN' in os.environ else sys.argv[1]


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='PhotoSliceBot')
    parser.add_argument('key')
    parser.add_argument('-i', '--host', default='', help='webhook: your IP address')
    parser.add_argument('-p', '--port', type=int, default=8443, help='webhook: port 443, 80, 88 or 8443')
    parser.add_argument('-s', '--shost', default='', help='server host')
    parser.add_argument('-o', '--sport', type=int, default=5001, help='server port')
    parser.add_argument('-a', '--admin_id', type=int, default=1234, help='admin user id')
    args = parser.parse_args()

    log.info("bot:\ntoken: %s\nhost: %s\nport: %d" % (TOKEN, args.host, args.port))

    #Telegram bot
    updater = Updater(token=TOKEN)

    updater.dispatcher.add_handler(conv_handler)
    updater.dispatcher.add_handler(MessageHandler([Filters.text], outside_handler))
    updater.dispatcher.add_error_handler(error_handler)

    if args.host:
        updater.start_webhook(listen=args.host,
                              port=args.port,
                              url_path=TOKEN,
                              key=CERT_KEY,
                              cert=CERT,
                              webhook_url='https://%s:8443/%s' % (args.host, TOKEN))
        log.info('webhook started')

    else:
        updater.start_polling()
        log.info('start polling')

    for sig in (SIGINT, SIGTERM, SIGABRT):
        signal(sig, updater.signal_handler)

    updater.is_idle = True

    #Tcp server
    h = Parser(updater.bot)
    server = TcpServer(args.shost, args.sport, h.handle_data)
    server.loop()

    log.info("correct exit")
    db.close()

