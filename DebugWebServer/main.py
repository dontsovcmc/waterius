import os
import uvicorn
from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from starlette.responses import FileResponse
from fastapi.encoders import jsonable_encoder
from fastapi.responses import JSONResponse
from fastapi.responses import RedirectResponse
from pydantic import BaseModel
from urllib.parse import urlencode
from log import log
from esp import settings, state
import time


ROOT_PATH = os.path.normpath(os.path.join(os.path.abspath(__file__),
                                          os.pardir, os.pardir, 'ESP8266', 'data'))
if not os.path.exists(ROOT_PATH):
    raise Exception(f'Не найдена папка с файлами: {ROOT_PATH}')
else:
    log.info(f'root path: {ROOT_PATH}')

app = FastAPI()
app.mount("/", StaticFiles(directory=ROOT_PATH), name="static")


@app.get("/")
async def root():
    return FileResponse(os.path.join(ROOT_PATH, 'index.html'))


@app.get("/networks")
async def networks():
    networks = [{"name": "HAUWEI-B311_F9E1", "level": 5},
                {"name": "ERROR_PASSWORD", "level": 4},
                {"name": "ZTE_8ds9f8sd67", "level": 3},
                {"name": "C78F56", "level": 2},
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


class Network(BaseModel):
    ssid: str
    password: str

    dhcp_on: bool | None = None
    gateway_ip: str | None = None
    static_ip: str | None = None
    mask: str | None = None
    mac_address: str | None = None


@app.get("/connect")
async def connect(network: Network):

    if network.ssid == "ERROR_PASSWORD":
        time.sleep(1.0)

        data = {
            "ssid": network.ssid,
            "password": network.password,
            "dhcp_on": settings.dhcp_on,
            "gateway_ip": settings.gateway_ip,
            "static_ip": settings.static_ip,
            "mask": settings.mask,
            "mac_address": settings.mac_address,
            "connect_error": "Ошибка авторизации. Проверьте пароль"
        }
        return RedirectResponse(url=f'/wifi-settings.html#{urlencode(data)}')
    else:
        settings.ssid = network.ssid
        settings.password = network.password

        time.sleep(1.0)
        return RedirectResponse(url='/meter-cold.html')


@app.get("/inputs_status")
async def inputs_status():
    status = {
        "state0": state.state0,
        "factor0": settings.factor0,
        "delta0": state.delta0,
        "state1": state.state1,
        "factor1": settings.factor1,
        "delta1": state.delta1,
    }
    json_status = jsonable_encoder(status)
    return JSONResponse(content=json_status)


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
