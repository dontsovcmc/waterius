# -*- coding: utf-8 -*-


from service import exception_info

from telegram import ReplyKeyboardMarkup, KeyboardButton
from telegram.ext import (Updater, CommandHandler, MessageHandler, Filters, RegexHandler,
                          ConversationHandler)

from storage import db
from logger import log

STATE_MENU, STATE_ADD, STATE_NEW_ID, STATE_COUNTER_LIST, STATE_REMOVE = range(5)


def outside_handler(bot, update):
    bot.sendMessage(update.message.chat_id,
                    text=u'Что-то пошло не так. Для возврата в меню нажмите /start')


def error_handler(bot, update, error):
    if update:
        user_id = update.message.from_user.id
    log.error('type_handler error: %s' % str(error))
    log.error(exception_info())


def start_handler(bot, update):
    reply_keyboard = [[KeyboardButton(u'Список'),
                       KeyboardButton(u'Добавить'),
                       KeyboardButton(u'Удалить')]]

    d = db.get_devices(update.message.chat_id)
    if d:
        text = u'Добавлены счетчики: '
        text += u', '.join(d)
    else:
        text = ''
    text += u'\nДля добавления нового счетчика нажмите \"Добавить\"'

    bot.sendMessage(update.message.chat_id,
                    text=text,
                    reply_markup=ReplyKeyboardMarkup(reply_keyboard,
                                                     resize_keyboard=True))
    return STATE_ADD


def add_handler(bot, update):

    bot.sendMessage(update.message.chat_id,
                    text=u'Введите номер с корпуса и нажмите отправить',
                    reply_markup=ReplyKeyboardMarkup([[KeyboardButton(u'Меню')]],
                                                     resize_keyboard=True))
    return STATE_NEW_ID


def delete_handler(bot, update):

    reply_keyboard = []
    for d in db.get_devices(update.message.chat_id):
        reply_keyboard.append([KeyboardButton(u'%s' % d)])

    reply_keyboard.append([KeyboardButton(u'Меню')])

    bot.sendMessage(update.message.chat_id,
                    text=u'Нажмите на кнопку, чтобы удалить счетчик',
                    reply_markup=ReplyKeyboardMarkup(reply_keyboard, resize_keyboard=True))

    return STATE_REMOVE


def list_handler(bot, update):
    reply_keyboard = []
    for d in db.get_devices(update.message.chat_id):
        reply_keyboard.append([KeyboardButton(u'Счетчик: %s' % d)])

    reply_keyboard.append([KeyboardButton(u'Меню')])

    bot.sendMessage(update.message.chat_id,
                    text=u'При нажатии на счетчик будут показаны данные',
                    reply_markup=ReplyKeyboardMarkup(reply_keyboard, resize_keyboard=True))

    return STATE_COUNTER_LIST


def confirm_delete_handler(bot, update):
    try:
        id = update.message.text
        if len(id) == 6 and int(id):
            db.remove_device(id, update.message.chat_id)
            bot.sendMessage(update.message.chat_id,
                            text=u'Удален счетчик #' + id)
    except Exception, err:
        pass
    return start_handler(bot, update)


def id_handler(bot, update):
    try:
        id = update.message.text
        if len(id) == 6 and int(id):
            db.add_device(id, update.message.chat_id)
            bot.sendMessage(update.message.chat_id,
                            text=u'Добавлен новый счетчик #' + id)
        return start_handler(bot, update)

    except Exception, err:
        return add_handler(bot, update)


def value_handler(bot, update):

    bot.sendMessage(update.message.chat_id,
                    text=u'Значения: #',
                    reply_markup=ReplyKeyboardMarkup([[KeyboardButton(u'Меню')]],
                                                     resize_keyboard=True))

    return STATE_COUNTER_LIST


conv_handler = ConversationHandler(
    entry_points=[CommandHandler('start', start_handler)],

    states={
        STATE_MENU: [CommandHandler('start', start_handler),
                     CommandHandler('add', add_handler)],

        STATE_ADD: [RegexHandler(u'^(Список)$', list_handler),
                    RegexHandler(u'^(Добавить)$', add_handler),
                    RegexHandler(u'^(Удалить)$', delete_handler),
                    MessageHandler([Filters.text], add_handler)],

        STATE_NEW_ID: [RegexHandler(u'^(Меню)$', start_handler),
                       MessageHandler([Filters.text], id_handler)],  # введен id

        STATE_REMOVE:  [RegexHandler(u'^(Меню)$', start_handler),
                       MessageHandler([Filters.text], confirm_delete_handler)],  #

        STATE_COUNTER_LIST:  [RegexHandler(u'^(Меню)$', start_handler),
                        MessageHandler([Filters.text], value_handler)],

    },

    fallbacks=[CommandHandler('exit', error_handler)],

    allow_reentry=True
)