# -*- coding: utf-8 -*-

import os
import sys
import argparse
from signal import signal, SIGINT, SIGTERM, SIGABRT

from server import TcpServer
from cparser import CounterParser
from telegram.ext import Updater, MessageHandler, Filters
from storage import db
from logger import log
from bot import conv_handler, error_handler, outside_handler, newid_handler
from telegram.ext import CommandHandler
from datetime import datetime, timedelta
from job import send_message_job

CERT = 'server.crt'
CERT_KEY = 'server.key'

# Token бота, полученный от BotFather
TOKEN = os.environ['BOT_TOKEN'] if 'BOT_TOKEN' in os.environ else sys.argv[1]


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='PhotoSliceBot')
    parser.add_argument('key')
    parser.add_argument('-i', '--host', default='', help='only for webhook: your IP address')
    parser.add_argument('-p', '--port', type=int, default=8443, help='only for webhook: port 443, 80, 88 or 8443')
    parser.add_argument('-s', '--shost', default='', help='TCP server host')
    parser.add_argument('-o', '--sport', type=int, default=5001, help='TCP server port')
    parser.add_argument('-u', '--admin', type=str, help='admin username for add new devices')
    args = parser.parse_args()

    log.info("bot:\nhost: %s\nport: %d\nshost: %s\nsport: %d" % (args.host, args.port, args.shost, args.sport))

    #Telegram bot
    updater = Updater(token=TOKEN)

    if args.admin:
        updater.dispatcher.add_handler(CommandHandler('newid', newid_handler, Filters.chat(username=args.admin)))
    updater.dispatcher.add_handler(conv_handler)
    updater.dispatcher.add_handler(MessageHandler([Filters.text], outside_handler))
    updater.dispatcher.add_error_handler(error_handler)

    tomorrow = datetime.now() + timedelta(1)
    afternoon = datetime(year=tomorrow.year, month=tomorrow.month,
                         day=tomorrow.day, hour=12, minute=0, second=0)
    next_job = (afternoon - datetime.now()).seconds
    log.info('send message job: after {} sec'.format(next_job))

    updater.job_queue.run_repeating(send_message_job, 3600*24, first=next_job)

    if args.host:
        updater.start_webhook(listen=args.host,
                              port=args.port,
                              url_path=TOKEN,
                              key=CERT_KEY,
                              cert=CERT,
                              webhook_url='https://%s:%d/%s' % (args.host, args.port, TOKEN))
        log.info('webhook started')

    else:
        updater.start_polling()
        log.info('start polling')

    for sig in (SIGINT, SIGTERM, SIGABRT):
        signal(sig, updater.signal_handler)

    updater.is_idle = True

    #Tcp server
    h = CounterParser(updater.bot)
    server = TcpServer(args.shost, args.sport, h.handle_data)
    server.loop()

    log.info("correct exit")
    db.close()

