
from fastapi import FastAPI
from esp import attiny_link_error, input0_settings, input1_settings
from copy import deepcopy

debug_app = FastAPI(title="debug application")

input0_runtime = deepcopy(input0_settings)
input1_runtime = deepcopy(input1_settings)


@debug_app.get("/input0")
async def input0():
    """
    ДЛЯ ТЕСТА: Замкнуть Вход 0 (Горячий счетчик)
    """
    input0_runtime.impulses += 1
    return str(input0_runtime.impulses)


@debug_app.get("/input1")
async def input1():
    """
    ДЛЯ ТЕСТА: Замкнуть Вход 1 (Холодный счетчик)
    """
    input1_runtime.impulses += 1
    return str(input1_runtime.impulses)


@debug_app.post("/attiny_error")
async def attiny_error():
    """
    Включить/выключить ошибку связи с attiny
    """
    global attiny_link_error
    attiny_link_error = not attiny_link_error
    return attiny_link_error


@debug_app.post("/set_error")
async def set_error():
    """
    Включить/выключить ошибку записи параметра в ESP
    """
    global set_error
    set_error = not set_error
    return set_error
