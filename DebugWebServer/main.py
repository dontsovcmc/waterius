
import uvicorn
from copy import deepcopy
from fastapi import FastAPI, Depends
from fastapi.staticfiles import StaticFiles
from fastapi.encoders import jsonable_encoder
from fastapi.responses import JSONResponse
from fastapi.responses import RedirectResponse
from urllib.parse import urlencode
from esp import settings, attiny_data, AttinyData
from api_debug import debug_app
from config import ROOT_PATH
from models import SettingsModel, ConnectModel
import time


api_app = FastAPI(title="api application")

app = FastAPI(title="main application")
app.mount("/api", api_app)
app.mount("/debug", debug_app)
app.mount("/", StaticFiles(directory=ROOT_PATH, html=True), name="static")


@api_app.get("/networks")
async def networks():
    networks = [{"name": "HAUWEI-B311_F9E1", "level": 5},
                {"name": "ERROR_PASSWORD", "level": 4},
                {"name": "ERROR_CONNECT", "level": 3},
                {"name": "OK", "level": 2},
                {"name": "C78F56_5G", "level": 1},
                {"name": "wifi-6", "level": 1},
                {"name": "wifi-7", "level": 1},
                {"name": "wifi-8", "level": 1},
                {"name": "wifi-9", "level": 1},
                {"name": "wifi-10", "level": 1},
                {"name": "wifi-11", "level": 1},
                {"name": "wifi-12", "level": 1},
                {"name": "wifi-13", "level": 1},
                {"name": "wifi-14", "level": 1}
    ]
    json_networks = jsonable_encoder(networks)
    return JSONResponse(content=json_networks)


@api_app.post("/set")
async def set(form_data: SettingsModel = Depends()):
    """
    Сохранение параметров с вебстраницы в память ESP
    :param form_data:
    :return:
    """
    for k, v in form_data:
        if settings.get(k):
            settings.set(k, v)
    return ''


@api_app.post("/connect")
async def connect(form_data: ConnectModel = Depends()):
    """
    Инициирует подключение ESP к Wi-Fi роутеру.
    Успешное подключение: /meter-cold.html
    Ошибка подключения: /wifi-settings.html#параметры_подключения
    :param form_data:
    :return:
    """
    data = deepcopy(form_data.__dict__)

    if form_data.ssid == "ERROR_PASSWORD":
        time.sleep(1.0)
        data["connect_error"] = "Ошибка авторизации. Проверьте пароль"
        return RedirectResponse(url=f'/wifi-settings.html#{urlencode(form_data.__dict__)}')

    elif form_data.ssid == "ERROR_CONNECT":
        time.sleep(1.0)
        data["connect_error"] = "Ошибка подключения"
        return RedirectResponse(url=f'/wifi-settings.html#{urlencode(form_data.__dict__)}')

    else:
        settings.ssid = form_data.ssid
        settings.password = form_data.password

        time.sleep(1.0)
        return RedirectResponse(url='/meter-cold.html')




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


@api_app.post("/turnoff")
async def turnoff():
    """
    Выключить ESP (Завершить настройку)
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


if __name__ == "__main__":
    uvicorn.run("main:app", host='0.0.0.0', port=8000, reload=True)
