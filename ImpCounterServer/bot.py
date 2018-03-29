# -*- coding: utf-8 -*-


from service import exception_info

from telegram import ReplyKeyboardMarkup, KeyboardButton, InlineKeyboardButton, InlineKeyboardMarkup
from telegram.ext import (Updater, CommandHandler, MessageHandler, Filters, RegexHandler,
                          ConversationHandler, CallbackQueryHandler)

from storage import db
from logger import log
from emoji import emojize
from storage import COLD_HOT, HOT_COLD

STATE_START, STATE_MENU, STATE_ADD_ID, STATE_ADD_PWD, STATE_DEVICE, STATE_INPUT_VALUE = range(6)

BLUE = emojize(':large_blue_circle:', use_aliases=True)  # https://www.webpagefx.com/tools/emoji-cheat-sheet/
RED = emojize(':red_circle:', use_aliases=True)
ARROWS = emojize(':arrows_counterclockwise:', use_aliases=True)

def outside_handler(bot, update):
    bot.sendMessage(update.message.chat_id,
                    text=u'Что-то пошло не так. Для возврата в меню нажмите /start')

def error_handler(bot, update, error):
    if update:
        if update.message:
            user_id = update.message.from_user.id
        elif update.callback_query:
            data = update.callback_query.data
    log.error('type_handler error: %s' % str(error))
    log.error(exception_info())


def start_handler(bot, update):

    query = update.callback_query
    chat_id = update.message.chat_id if update.message else query.message.chat_id
    d = db.get_devices(chat_id)
    text = '' if not d else u'Добавлены счетчики: %s' % u', '.join(d)
    text = u'Народный wi-fi счетчик воды \n' + text

    if query:
        bot.editMessageText(
            message_id=query.message.message_id,
            chat_id=query.message.chat.id,
            text=text,
            inline_message_id=query.message.message_id,
            reply_markup=InlineKeyboardMarkup([[InlineKeyboardButton(u'Меню', callback_data=u'Меню')]]))
    else:
        bot.sendMessage(update.message.chat_id,
            text=text,
            reply_markup=InlineKeyboardMarkup([[InlineKeyboardButton(u'Меню', callback_data=u'Меню')]]))

    return STATE_MENU


def menu_handler(bot, update):
    keyboard = [InlineKeyboardButton(u'Выход', callback_data=u'Выход')]
    query = update.callback_query
    chat_id = query.message.chat_id
    if query.data == u'Меню':
        keyboard = []
        for d in db.get_devices(chat_id):
            keyboard.append(InlineKeyboardButton(u'%s' % d, callback_data=u'%s' % d))

        keyboard.append(InlineKeyboardButton(u'Добавить', callback_data=u'Добавить'))
        keyboard.append(InlineKeyboardButton(u'Выход', callback_data=u'Выход'))

        bot.editMessageText(
            message_id=query.message.message_id,
            chat_id=query.message.chat.id,
            text='Меню:',
            inline_message_id=query.message.message_id,
            reply_markup=InlineKeyboardMarkup([keyboard]))
        return STATE_MENU

    elif query.data == u'Выход':
        return start_handler(bot, update)

    elif query.data == u'Добавить':
        bot.editMessageText(
            message_id=query.message.message_id,
            chat_id=query.message.chat.id,
            text=u'Введите ID счетчика и нажмите отправить:',
            inline_message_id=query.message.message_id,
            reply_markup=InlineKeyboardMarkup([keyboard]))
        return STATE_ADD_ID

    else:
        try:
            if int(query.data):
                id = query.data
                db.set_select_id(chat_id, id)
                return device_menu(bot, update, id)
        except Exception, err:
            log.error(str(err))
    return STATE_START


def device_menu(bot, update, id):

    factor = db.get_factor(id)
    v1, v2 = db.get_current_value(id)
    imp1, imp2 = db.get_impulses(id)
    pwd = db.get_pwd(id)

    order = db.get_order(id, COLD_HOT)

    if order == COLD_HOT:
        order_button = InlineKeyboardButton(ARROWS + u' ' + BLUE + u'ХВС ' + RED + u'ГВС', callback_data=u'ГВС  ХВС')
    else:
        order_button = InlineKeyboardButton(ARROWS + u' ' + RED + u'ГВС ' + BLUE + u'ХВС', callback_data=u'ХВС  ГВС')

    keyboard = [
        [InlineKeyboardButton(u'%dимп/л' % factor, callback_data=u'Множитель'),
         InlineKeyboardButton(RED + u'{0:.3f} '.format(v1) + BLUE + u'{0:.3f}'.format(v2), callback_data=u'Значение')],
        [InlineKeyboardButton(u'Текст СМС', callback_data=u'Текст СМС'), order_button],
        [InlineKeyboardButton(u'Удалить', callback_data=u'Удалить'), InlineKeyboardButton(u'Выход', callback_data=u'Выход')]
    ]

    query = update.callback_query
    if query:
        bot.editMessageText(
                message_id=query.message.message_id,
                chat_id=query.message.chat_id,
                text=u'Счетчик #%s (%s) _ [%d, %d]' % (id, pwd, imp1, imp2),
                inline_message_id=query.message.message_id,
                reply_markup=InlineKeyboardMarkup(keyboard))
    else:
        bot.sendMessage(update.message.chat_id,
                text=u'Счетчик #%s (%s) _ [%d, %d]' % (id, pwd, imp1, imp2),
                reply_markup=InlineKeyboardMarkup(keyboard))

    return STATE_DEVICE


def add_id_handler(bot, update):

    try:
        chat_id = update.message.chat_id
        keyboard = [InlineKeyboardButton(u'Выход', callback_data=u'Выход')]
        id = update.message.text
        if len(id) <= 5 and int(id):
            db.set_select_id(chat_id, id)
            text = u'Введите пароль от счетчика: %s' % id
            bot.sendMessage(chat_id, text=text, reply_markup=InlineKeyboardMarkup([keyboard]))
            return STATE_ADD_PWD

    except Exception, err:
        log.error("add_id_handler error: " % str(err))
    return STATE_START


def add_pwd_handler(bot, update):
    try:
        chat_id = update.message.chat_id
        keyboard = [InlineKeyboardButton(u'Меню', callback_data=u'Меню')]
        pwd = update.message.text

        id = db.selected_id(chat_id)

        if len(pwd) <= 5 and int(pwd):

            if db.check_pwd(id, pwd):

                db.add_device(id, chat_id)

                bot.sendMessage(chat_id,
                                text=u'Новый счетчик добавлен #%s' % id)
                return device_menu(bot, update, id)

        bot.sendMessage(chat_id,
                        text=u'Пароль не корректный\nВведите пароль от счетчика: %s' % id,
                        reply_markup=InlineKeyboardMarkup([keyboard]))
        return STATE_ADD_PWD

    except Exception, err:
        log.error("add_pwd_handler error: " % str(err))
        return start_handler(bot,update)


def device_handler(bot, update):

    keyboard = []
    query = update.callback_query

    if not query:
        chat_id = update.message.chat_id
        id = db.selected_id(chat_id)
        return device_menu(bot, update, id)

    chat_id = query.message.chat_id
    id = db.selected_id(chat_id)

    if query.data == u'Выход':
        return start_handler(bot, update)

    elif query.data == u'Значение':
        bot.editMessageText(
            message_id=query.message.message_id,
            chat_id=query.message.chat.id,
            text=u'Введите текущие значения через пробел ' + RED + u'ГВС,' + BLUE + u' ХВС: ',
            inline_message_id=query.message.message_id,
            reply_markup=InlineKeyboardMarkup([keyboard]))
        return STATE_INPUT_VALUE

    elif query.data == u'Текст СМС':
        text = db.sms_text(id)
        bot.sendMessage(chat_id, text=text)
        return start_handler(bot, update)

    elif query.data == u'Множитель':
        factor = db.get_factor(id)

        def choosen(data, selected):
            s = u'1имп = %dл' % data
            return u'* %s *' % s if data == selected else s

        keyboard = [
            [InlineKeyboardButton(choosen(1, factor), callback_data=u'1')],
            [InlineKeyboardButton(choosen(10, factor), callback_data=u'10')],
            [InlineKeyboardButton(choosen(100, factor), callback_data=u'100')],
            [InlineKeyboardButton(choosen(1000, factor), callback_data=u'1000')]
        ]

        bot.editMessageText(
            message_id=query.message.message_id,
            chat_id=query.message.chat.id,
            text=u'Выберите множитель:',
            inline_message_id=query.message.message_id,
            reply_markup=InlineKeyboardMarkup(keyboard))
        return STATE_DEVICE

    elif query.data == u'1':
        db.set_factor(id, 1)
    elif query.data == u'10':
        db.set_factor(id, 10)
    elif query.data == u'100':
        db.set_factor(id, 100)
    elif query.data == u'1000':
        db.set_factor(id, 1000)

    elif query.data == u'ХВС  ГВС':
        db.set_order(id, COLD_HOT)
    elif query.data == u'ГВС  ХВС':
        db.set_order(id, HOT_COLD)

    elif query.data == u'да':
        db.remove_device(id, chat_id)
        return start_handler(bot, update)

    elif query.data == u'нет':
        return device_menu(bot, update, id)

    elif query.data == u'Удалить':
        keyboard.append(InlineKeyboardButton(u'да', callback_data=u'да'))
        keyboard.append(InlineKeyboardButton(u'нет', callback_data=u'нет'))
        bot.editMessageText(
            message_id=query.message.message_id,
            chat_id=query.message.chat.id,
            text=u'Вы уверены?',
            inline_message_id=query.message.message_id,
            reply_markup=InlineKeyboardMarkup([keyboard]))
        return STATE_DEVICE

    return device_menu(bot, update, id)


def input_value(bot, update):
    chat_id = update.message.chat_id
    device_id = db.selected_id(chat_id)
    id = db.selected_id(chat_id)
    factor = db.get_factor(device_id)
    try:
        v1, v2 = update.message.text.split(' ')
        db.set_start_value1(device_id, float(v1))
        db.set_start_value2(device_id, float(v2))
        return device_menu(bot, update, id)

    except Exception, err:
        text = u'Ошибка {}'.format(err)
        bot.sendMessage(chat_id, text=text)
        return device_menu(bot, update, id)


# Пользователь ввел команду /newid
def newid_handler(bot, update):

    id, pwd = db.generate_id()

    bot.sendMessage(update.message.chat_id, text="Новое устройство зарегистрировано\n"
                                                 "Уникальный ID: %s\n"
                                                 "Пароль: %s" % (id, pwd))
    return start_handler(bot, update)


conv_handler = ConversationHandler(
    entry_points=[CommandHandler('start', start_handler)],

    states={
        STATE_START: [CommandHandler('start', start_handler)],

        STATE_MENU: [CallbackQueryHandler(menu_handler),
                     MessageHandler(Filters.all, start_handler)],  #menu

        STATE_ADD_ID: [MessageHandler(Filters.all, add_id_handler),
                       CallbackQueryHandler(start_handler)],  #menu

        STATE_ADD_PWD: [MessageHandler(Filters.all, add_pwd_handler),
                        CallbackQueryHandler(start_handler)],  #menu

        STATE_DEVICE: [CallbackQueryHandler(device_handler),
                       MessageHandler(Filters.all, device_handler)],  #id

        STATE_INPUT_VALUE: [MessageHandler(Filters.all, input_value)]
    },

    fallbacks=[CommandHandler('exit', error_handler)],

    allow_reentry=True
)

