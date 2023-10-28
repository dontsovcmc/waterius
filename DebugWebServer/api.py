
from fastapi import FastAPI, Depends
from fastapi.encoders import jsonable_encoder
from fastapi.responses import JSONResponse
from esp import settings, attiny_data, attiny_link_error, AUTO_IMPULSE_FACTOR, AS_COLD_CHANNEL
from api_debug import runtime_data
from models import SettingsModel, ConnectModel
import time


api_app = FastAPI(title="api application")


@api_app.get("/networks")
async def networks():
    networks = [{"ssid": "HAUWEI-B311_F9E1", "level": 5, "wifi_channel": 1},
                {"ssid": "ERROR_PASSWORD", "level": 4, "wifi_channel": 1},
                {"ssid": "ERROR_CONNECT", "level": 3, "wifi_channel": 1},
                {"ssid": "OK", "level": 2, "wifi_channel": 1},
                {"ssid": "C78F56_5G", "level": 1, "wifi_channel": 1},
                {"ssid": "wifi-6", "level": 1, "wifi_channel": 6},
                {"ssid": "wifi-7", "level": 1, "wifi_channel": 7},
                {"ssid": "wifi-8", "level": 1, "wifi_channel": 8},
                {"ssid": "wifi-9", "level": 1, "wifi_channel": 9},
                {"ssid": "wifi-10", "level": 1, "wifi_channel": 10},
                {"ssid": "wifi-11", "level": 1, "wifi_channel": 11},
                {"ssid": "wifi-12", "level": 1, "wifi_channel": 12},
                {"ssid": "wifi-13", "level": 1, "wifi_channel": 13},
                {"ssid": "wifi-14", "level": 1, "wifi_channel": 13}
                ]
    json_networks = jsonable_encoder(networks)
    return JSONResponse(content=json_networks)


@api_app.post("/connect")
async def connect(form_data: ConnectModel = Depends()):
    """
    Инициирует подключение ESP к Wi-Fi роутеру.
    Успешное подключение: /setup_cold_welcome.html
    Ошибка подключения: /wifi_settings.html#параметры_подключения
    :param form_data:
    :return:
    """
    res = {k: v for k, v in form_data.__dict__.items() if v is not None}

    ap_channel = settings.wifi_channel

    res = settings.apply_settings(res)

    if settings.wifi_channel != ap_channel:
        # При подключении к Роутеру на другой частоте связь с точкой доступа оборвется
        res.update({
            "redirect": "/wifi_connect.html?error=Канал Wi-Fi роутера отличается от текущего соединения. Если телефон потеряет связь с Ватериусом, подключитесь заново."
        })
    else:
        res.update({
            "redirect": "/wifi_connect.html"
        })

    json = jsonable_encoder(res)
    return JSONResponse(content=json)


@api_app.get("/main_status")
async def main_status():
    """
    Запрос на главной странице для отображения диагностических сообщений.
    :return:
    """
    res = [{
        "error": "Счетчик холодной воды перестал передавать показания",
        "link_text": "Настроить заново",
        "link": "/setup_cold_welcome.html"
    }, {
        "error": "Подключите заново провода холодного счётчика к Ватериусу",
        "link_text": "Приступить к настройке",
        "link": "/setup_cold_welcome.html"
    }, {
        "error": "Расход холодной воды больше горячей в 30 раз",
        "link_text": "Настроить множитель",
        "link": "/setup_cold.html"
    }]

    res = []

    json = jsonable_encoder(res)
    return JSONResponse(content=json)


@api_app.get("/connect_status")
async def connect_status():
    """
    Запрос странице подключения. Результат подключения будет в redirect

    typedef enum {
        WL_IDLE_STATUS      = 0,
        WL_NO_SSID_AVAIL    = 1,
        WL_SCAN_COMPLETED   = 2,
        WL_CONNECTED        = 3,
        WL_CONNECT_FAILED   = 4,
        WL_CONNECTION_LOST  = 5,
        WL_WRONG_PASSWORD   = 6,
        WL_DISCONNECTED     = 7
    } wl_status_t;
    :return:
    """
    if settings.ssid == "ERROR_PASSWORD":
        res = {
            "redirect": "/wifi_settings.html",
            "params": "status_code=6"
        }
    elif settings.ssid == "ERROR_CONNECT":
        res = {
            "redirect": "/wifi_settings.html",
            "params": "status_code=4"
        }
    else:
        res = {"redirect": "/setup_send.html"}

    json = jsonable_encoder(res)
    return JSONResponse(content=json)

"""
@api_app.get("/status/{input}")
async def status_input(input: int):
    #res = {"state": 0, "factor": 1, "delta": 2, "error": ""}
    res = {"state": 0, "factor": 0, "delta": 2, "error": ""}  # Подключен
    #res = {"state": 0, "factor": 1, "delta": 2, "error": "Ошибка связи с МК"}
    json = jsonable_encoder(res)
    return JSONResponse(content=json)

"""


@api_app.get("/status/{input}")
async def status(input: int):
    """
    Запрос данных Входа
    :param input: 0 или 1
    :return: JSON
        state: 0 - не подключен, 1 - подключен
        factor: множитель
        delta: количество импульсов пришедших с момента настройки
        error: если есть ошибка связи с МК (имитация: /attiny_link)
    """
    global attiny_link_error
    error_str = "" if not attiny_link_error else "Ошибка связи с МК"

    if input == 0:  # ГВС

        if settings.factor0 == AS_COLD_CHANNEL:
            factor = settings.factor1  # как у холодной
        else:
            factor = settings.factor0

        status = {
            "state": int(runtime_data.impulses0 > attiny_data.impulses0),
            "factor": factor,
            "impulses": runtime_data.impulses0 - attiny_data.impulses0,
            "error": error_str
        }
    elif input == 1:  # ХВС

        if settings.factor1 == AUTO_IMPULSE_FACTOR:
            # если первичная настройка, то множитель определяем по слитой воде
            factor = 1 if runtime_data.impulses1 - attiny_data.impulses1 > 2 else 10
        else:
            factor = settings.factor1

        status = {
            "state": int(runtime_data.impulses1 > attiny_data.impulses1),
            "factor": factor,
            "impulses": runtime_data.impulses1 - attiny_data.impulses1,
            "error": error_str
        }
    else:
        status = {"error": "Некорректные данные {input}"}

    json_status = jsonable_encoder(status)
    return JSONResponse(content=json_status)


@api_app.post("/setup")
async def setup(form_data: SettingsModel = Depends()):
    res = {k: v for k, v in form_data.__dict__.items() if v is not None}
    res = settings.apply_settings(res)

    # res["errors"]["form"] = "Ошибка формы сообщение"
    # res["errors"]["serial1"] = "Введите серийный номер"
    # res["errors"]["channel1_start"] = "Введите показания счётчика"
    #  "redirect": "/finish.html"

    json = jsonable_encoder(res)
    return JSONResponse(content=json)


@api_app.get("/turnoff")
async def turnoff():
    """
    Выключить ESP (Завершить настройку)
    1. ESP отключает свою точку доступа
    2. Телефон ищет родной Вай-фай или сеть оператора
    3. Т.к. страница открыла waterius.ru/account, то браузер ее загрузит, как появится сеть
    :return:
    """
    return ''


@api_app.post("/reset")
async def reset():
    """
    Возврат к заводским настройкам ESP
    :return:
    """
    return ''

