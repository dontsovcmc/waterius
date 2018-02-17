

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
from flask import Flask, request
from signal import signal, SIGINT, SIGTERM, SIGABRT

from telegram.ext import Updater, CommandHandler
from storage import db
from logger import log
from bot import start_handler, add_handler

CERT     = 'server.crt'
CERT_KEY = 'server.key'

# CONFIG
TOKEN = os.environ['BOT_TOKEN'] if 'BOT_TOKEN' in os.environ else sys.argv[1]

app = Flask(__name__)


@app.route('/log/<int:device_id>', methods=['POST'])
def log_message(device_id):
    # show the user profile for that user
    print 'device %d' % device_id
    print request.get_data()
    return 'OK'


@app.route('/data/add/<int:device_id>', methods=['POST'])
def add_data(device_id):
    content = request.get_json(force=True, silent=True)
    print 'Device %d data:\n%s' % (device_id, content)
    return 'OK'


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='PhotoSliceBot')
    parser.add_argument('key')
    parser.add_argument('-i', '--host', default='', help='webhook: your IP address')
    parser.add_argument('-p', '--port', type=int, default=8443, help='webhook: port 443, 80, 88 or 8443')
    parser.add_argument('-s', '--shost', default='', help='server host')
    parser.add_argument('-o', '--sport', type=int, default=5001, help='server port')
    parser.add_argument('-a', '--admin_id', type=int, default=1234, help='admin user id')
    args = parser.parse_args()

    log.info("BOT:\ntoken: %s\nhost: %s\nport: %d" % (TOKEN, args.host, args.port))

    updater = Updater(token=TOKEN)
    updater.dispatcher.add_handler(CommandHandler('start', start_handler))
    updater.dispatcher.add_handler(CommandHandler('add', add_handler))

    if args.host:
        updater.start_webhook(listen=args.host,
                              port=args.port,
                              url_path=TOKEN,
                              key=CERT_KEY,
                              cert=CERT,
                              webhook_url='https://%s:8443/%s' % (args.host, TOKEN))

        #updater.bot.set_webhook(url='https://%s:8443/%s/' % (args.host, TOKEN))
        #                                certificate=open(CERT, 'rb'))
        log.info('webhook started')

    else:
        updater.start_polling()
        log.info('start polling')

    #updater.idle()

    for sig in (SIGINT, SIGTERM, SIGABRT):
        signal(sig, updater.signal_handler)

    updater.is_idle = True

    app.run(host=args.shost,
            port=args.sport,
            #ssl_context=(CERT, CERT_KEY),
            #debug=True
    )

    log.info("correct exit")
    db.close()

