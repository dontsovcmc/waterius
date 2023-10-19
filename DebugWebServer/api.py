
from fastapi import FastAPI, Depends
from fastapi.encoders import jsonable_encoder
from fastapi.responses import JSONResponse
from esp import settings
from models import SettingsModel, ConnectModel
import time


api_app = FastAPI(title="api application")


@api_app.get("/networks")
async def networks():
    networks = [{"ssid": "HAUWEI-B311_F9E1", "level": 5},
                {"ssid": "ERROR_PASSWORD", "level": 4},
                {"ssid": "ERROR_CONNECT", "level": 3},
                {"ssid": "OK", "level": 2},
                {"ssid": "C78F56_5G", "level": 1},
                {"ssid": "wifi-6", "level": 1},
                {"ssid": "wifi-7", "level": 1},
                {"ssid": "wifi-8", "level": 1},
                {"ssid": "wifi-9", "level": 1},
                {"ssid": "wifi-10", "level": 1},
                {"ssid": "wifi-11", "level": 1},
                {"ssid": "wifi-12", "level": 1},
                {"ssid": "wifi-13", "level": 1},
                {"ssid": "wifi-14", "level": 1}
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
    res = {k: v for k, v in form_data.__dict__.items() if v}

    res = settings.apply_settings(res)

    if form_data.ssid == "ERROR_PASSWORD":
        res.update({
            "error": "Ошибка авторизации. Проверьте пароль",
            "redirect": "/wifi_settings.html"
        })

    elif form_data.ssid == "ERROR_CONNECT":
        res.update({
            "error": "Ошибка подключения",
            "redirect": "/wifi_settings.html"
        })
    else:
        pass

    time.sleep(1.0)
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

    json = jsonable_encoder(res)
    return JSONResponse(content=json)


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

    Запрос данных Входа
    :param input: 0 или 1
    :return: JSON
        state: 0 - не подключен, 1 - подключен
        factor: множитель
        delta: количество импульсов пришедших с момента настройки
        error: если есть ошибка связи с МК (имитация: /attiny_link)

    global attiny_link_error
    error_str = ""
    if attiny_link_error:
        error_str = "Ошибка связи с МК"

    if input == 0:
        status = {
            "state": int(runtime_data.impulses0 > attiny_data.impulses0),
            "factor": settings.factor0,
            "delta": runtime_data.impulses0 - attiny_data.impulses0,
            "error": error_str
        }
    elif input == 1:
        status = {
            "state": int(runtime_data.impulses1 > attiny_data.impulses1),
            "factor": settings.factor1,
            "delta": runtime_data.impulses1 - attiny_data.impulses1,
            "error": error_str
        }
    else:
        status = {"error": "Некорректные данные {input}"}

    json_status = jsonable_encoder(status)
    return JSONResponse(content=json_status)
"""


@api_app.post("/setup")
async def setup(form_data: SettingsModel = Depends()):
    res = {k: v for k, v in form_data.__dict__.items() if v}
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
