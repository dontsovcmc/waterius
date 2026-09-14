"""
Числа прошивки и стенда - в одном месте.

Тесты сверяют поведение с прошивкой, и почти все числа здесь - её числа: типы
входов, спецзначения веса, коды ошибок портала, биты масок. Копия в каждом
тесте означала бы однажды поправить одну и не заметить остальные. У каждой -
исходник, откуда она взята.

Параметры сценариев - сколько импульсов подать, какой порог выставить - сюда не
входят: они принадлежат тесту и объясняются рядом с ним.
"""

from __future__ import annotations

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# Входы (core/types.h): INPUT0_RED - красный, по умолчанию ГВС;
# INPUT1_BLUE - синий, ХВС. «Авто» вес живёт у синего
HOT = 0
COLD = 1

# Тип входа, core/types.h: CounterType
NAMUR = 0              # механический: геркон, сухой контакт, namur
ELECTRONIC = 2         # электронный выход, импульс - замыкание на минус
ELECTRONIC_HIGH = 4    # электронный выход, импульс - подъём линии
LEAKAGE = 5            # датчик протечки: замыкание - тревога, а не импульс
LEAKAGE_NC = 6         # нормально-замкнутый датчик: тревога - размыкание
INPUT_OFF = 255        # CounterType::NONE, вход выключен

# Ресурс входа, core/types.h: CounterName
WATER_COLD = 0
WATER_HOT = 1
ELECTRO = 2            # у электричества вес - импульсы на кВт*ч
HEAT_GCAL = 4
HEAT_KWT = 7

# core/ha_units.h. Единица - не украшение: по ней Home Assistant считает
# статистику, и перепутанная превращает показания в другие числа
HEAT_UNITS = {HEAT_GCAL: 'Gcal', HEAT_KWT: 'kWh'}

# Спецзначения веса импульса, core/types.h. Прошивка разворачивает их в число
# в момент применения (core/readings.cpp, get_auto_factor)
AUTO_IMPULSE_FACTOR = 3
AS_COLD_CHANNEL = 7
# core/readings.h: IMPULS_LIMIT_1. До стольких импульсов включительно «Авто»
# даёт 10 л/имп, дальше - 1 л/имп
AUTO_LIMIT = 3
AUTO_FACTOR_FEW = 10
AUTO_FACTOR_MANY = 1

# Вес, с которым стенд держит входы (stand.BASELINE)
BASE_FACTOR = 10

# Порог объёма, которым стенд вооружает тревоги: 50 л за полчаса при весе 10 -
# это пять импульсов. Темп значения не имеет, окно скользящее
VOL_LITRES = 50
VOL_PULSES = 5

# Умолчания после заводского сброса, config.cpp: init_config
DEFAULT_WAKEUP_PERIOD_MIN = 1440

# В режиме «Я уехал» порог объёма - один импульс (core/alarm.cpp, alarm_thresholds)
VACATION_PULSES = 1

# Коды ошибок полей портала, core/input.h: ParamError
ERR_LENGTH = '14'
ERR_VALUE = '15'
ERR_NO_COMMA = '19'
ERR_TLS = '20'
ERR_PORT_IN_HOST = '21'

# Коды плашек главной страницы (active_point_api.cpp, get_api_main_status)
FACTOR_TOO_BIG = '23'
INPUT_SILENT = '24'

# core/types.h: SERIAL_LEN. Длина - в байтах, кириллица занимает по два
SERIAL_LEN = 16

# core/idle.h: потолок остановки расхода в часах
ALARM_STOP_MAX_HOURS = 1092

# portal/resources.h: PARAM_ASTERICS - так интерфейс показывает сохранённый пароль
MASKED = '********'

# Маска снятия тревог в кадре 'A': биты 0-2 - канал 0, биты 3-5 - канал 1
# (core/types.h, ALARM_RESET_SHIFT1, ALARM_RESET_ALL)
RESET_FLOW1 = 0x08
RESET_WET1 = 0x20
RESET_ALL = 0x3F

# Бит маски квитанции тревоги, core/types.h: AlarmConfirm
CONFIRM_MQTT = 4

# Сколько ждём внепланового сеанса и сколько наблюдаем тишину. Пауза после
# сеанса по тревоге - ALARM_HOLD_MIN, пять минут (Attiny85/src/alarm.h), и
# раньше неё сеанс невозможен физически. Берём её плюс сеанс и запас
ALARM_WAIT_S = 420.0
SILENCE_S = 420.0

# Период на время теста, которому нужен плановый сеанс вместо кнопки: кнопка
# снимает тревоги сама (Attiny85/src/main.cpp, ButtonPressType::SHORT), и
# проверять ею снятие чем-то другим нельзя. Ждём такой сеанс с запасом на
# три периода
PLANNED_PERIOD_MIN = 5
PLANNED_WAIT_S = 15 * 60

# core/types.h: WATERIUS_MODEL_2
WATERIUS_MODEL_2 = 2

# ota_update.h
OTA_MIN_VOLTAGE_MV = 3300

# core/types.h, enum OtaError
OTA_ERR_NONE = 0
OTA_ERR_FW_UPDATE = 3
OTA_ERR_LOW_BATTERY = 4

# Умолчание прошивки: с этим значением пользовательский сервер не используется
# и время берётся из пула (ESP8266/src/sync_time.cpp, sync_ntp_time)
DEFAULT_NTP_SERVER = 'ru.pool.ntp.org'
NTP_POOL_SIZE = 4          # core/timekeeping.h
NTP_WARMUP_SYNCS = 2       # столько синхронизаций подряд, потом раз в сутки
START_VALID_TIME = 1704067201  # core/timekeeping.h: время раньше - негодное

# sender_http.h: столько раз прошивка повторяет отправку, пока не получит 200
HTTP_SEND_ATTEMPTS = 3

# core/portal_watchdog.h: портал закрывается через столько секунд без действий
PORTAL_WATCHDOG_S = 600
