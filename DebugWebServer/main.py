import uvicorn
import os
from typing import List, Any
from fastapi.responses import HTMLResponse, FileResponse
from fastapi import FastAPI, HTTPException
from fastapi.staticfiles import StaticFiles
from esp import settings, settings_vars, system_info, system_info_vars
from api import api_app
from api_debug import debug_app
from log import log


ROOT_PATH = os.path.normpath(os.path.join(os.path.abspath(__file__),
                                          os.pardir, os.pardir, 'ESP8266', 'data'))
if not os.path.exists(ROOT_PATH):
    raise Exception(f'Не найдена папка с файлами: {ROOT_PATH}')


app = FastAPI(title="main application")
app.mount("/api", api_app)
app.mount("/debug", debug_app)
app.mount("/images", StaticFiles(directory=os.path.join(ROOT_PATH, 'images')), name="static")


def replace_variables(text: str, keywords: List[str], obj: Any):
    for name in keywords:
        if f'%{name}%' in text:
            v = getattr(obj, name)
            if isinstance(v, bool):
                if v:
                    text = text.replace(f'%{name}%', 'value="1" checked')
                else:
                    text = text.replace(f'%{name}%', '')
            else:
                text = text.replace(f'%{name}%', str(v))
    return text


def template_response(filename: str):
    try:
        with open(os.path.join(ROOT_PATH, filename), "r") as file:
            html_content = file.read()

            html_content = replace_variables(html_content, settings_vars, settings)
            html_content = replace_variables(html_content, system_info_vars, system_info)

        return HTMLResponse(content=html_content)
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="File not found")


@app.get("/about.html")
async def about():
    return template_response("about.html")


@app.get("/style.css")
async def style():
    return FileResponse(os.path.join(ROOT_PATH, "style.css"))


@app.get("/common.js")
async def common():
    return FileResponse(os.path.join(ROOT_PATH, "common.js"))


@app.get("/favicon.ico")
async def favicon():
    return FileResponse(os.path.join(ROOT_PATH, "favicon.ico"))


@app.get("/finish.html")
async def finish():
    return template_response("finish.html")


@app.get("/")
async def index():
    return template_response("index.html")


@app.get("/logs.html")
async def logs():
    return template_response("logs.html")


@app.get("/reset.html")
async def reset():
    return template_response("reset.html")


@app.get("/setup_cold_welcome.html")
async def setup_cold_welcome():
    return template_response("setup_cold_welcome.html")


@app.get("/setup_cold.html")
async def setup_cold():
    return template_response("setup_cold.html")


@app.get("/setup_hot_welcome.html")
async def setup_hot_welcome():
    return template_response("setup_hot_welcome.html")


@app.get("/setup_hot.html")
async def setup_hot():
    return template_response("setup_hot.html")


@app.get("/setup_send.html")
async def setup_send():
    return template_response("setup_send.html")


@app.get("/start.html")
async def start():
    return template_response("start.html")


@app.get("/waterius_logs.txt")
async def waterius_logs():
    return FileResponse(os.path.join(ROOT_PATH, "waterius_logs.txt"))


@app.get("/wifi_list.html")
async def wifi_list():
    return template_response("wifi_list.html")


@app.get("/wifi_password.html")
async def wifi_password():
    return template_response("wifi_password.html")


@app.get("/wifi_settings.html")
async def wifi_settings():
    return template_response("wifi_settings.html")


if __name__ == "__main__":
    log.info(f'api: http://127.0.0.1:9000/api/docs')
    log.info(f'debug api: http://127.0.0.1:9000/debug/docs')
    uvicorn.run("main:app", host='0.0.0.0', port=7000, reload=True)
    #uvicorn.run("main:app", host='192.168.10.43', port=9000, reload=True)

