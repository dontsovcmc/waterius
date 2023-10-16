import uvicorn
from dataclasses import fields
from log import log
from copy import deepcopy
from fastapi.responses import HTMLResponse, FileResponse
from fastapi import FastAPI, Depends, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.encoders import jsonable_encoder
from fastapi.responses import JSONResponse
from fastapi.responses import RedirectResponse
from urllib.parse import urlencode
from esp import settings, settings_vars, attiny_data
from api import api_app
from api_debug import debug_app, runtime_data
from models import SettingsModel, ConnectModel
import time

import os


ROOT_PATH = os.path.normpath(os.path.join(os.path.abspath(__file__),
                                          os.pardir, os.pardir, 'ESP8266', 'data'))
if not os.path.exists(ROOT_PATH):
    raise Exception(f'Не найдена папка с файлами: {ROOT_PATH}')


app = FastAPI(title="main application")
app.mount("/api", api_app)
app.mount("/debug", debug_app)
app.mount("/images", StaticFiles(directory=os.path.join(ROOT_PATH, 'images')), name="static")


def template_response(filename: str):
    try:
        with open(os.path.join(ROOT_PATH, filename), "r") as file:
            html_content = file.read()

            for name in settings_vars:
                if f'%{name}%' in html_content:
                    v = getattr(settings, name)
                    if isinstance(v, bool):
                        if v:
                            html_content = html_content.replace(f'%{name}%', '1')
                        else:
                            html_content = html_content.replace(f'%{name}%', '0')
                    else:
                        html_content = html_content.replace(f'%{name}%', str(v))

        return HTMLResponse(content=html_content)
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="File not found")


@app.get("/style.css")
async def index():
    return FileResponse(os.path.join(ROOT_PATH, "style.css"))


@app.get("/common.js")
async def index():
    return FileResponse(os.path.join(ROOT_PATH, "common.js"))


@app.get("/favicon.ico")
async def index():
    return FileResponse(os.path.join(ROOT_PATH, "favicon.ico"))


@app.get("/finish.html", response_class=HTMLResponse)
async def index():
    return template_response("finish.html")


@app.get("/")
async def index():
    return template_response("index.html")


@app.get("/logs.html", response_class=HTMLResponse)
async def index():
    return template_response("logs.html")


@app.get("/reset.html", response_class=HTMLResponse)
async def index():
    return template_response("reset.html")


@app.get("/setup_cold_welcome.html", response_class=HTMLResponse)
async def index():
    return template_response("setup_cold_welcome.html")


@app.get("/setup_cold.html", response_class=HTMLResponse)
async def index():
    return template_response("setup_cold.html")


@app.get("/setup_hot_welcome.html", response_class=HTMLResponse)
async def index():
    return template_response("setup_hot_welcome.html")


@app.get("/setup_hot.html", response_class=HTMLResponse)
async def index():
    return template_response("setup_hot.html")


@app.get("/start.html", response_class=HTMLResponse)
async def index():
    return template_response("start.html")


@app.get("/wifi_list.html", response_class=HTMLResponse)
async def index():
    return template_response("wifi_list.html")


@app.get("/wifi_password.html", response_class=HTMLResponse)
async def index():
    return template_response("wifi_password.html")


@app.get("/wifi_settings.html", response_class=HTMLResponse)
async def index():
    return template_response("wifi_settings.html")


if __name__ == "__main__":
    uvicorn.run("main:app", host='0.0.0.0', port=9000, reload=True)
