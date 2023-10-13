
from fastapi import FastAPI
from esp import AttinyData

debug_app = FastAPI(title="debug application")


runtime_data = AttinyData()


@debug_app.get("/input0")
async def input0():
    """
    ДЛЯ ТЕСТА: Замкнуть Вход 0 (Горячий счетчик)
    """
    runtime_data.impulses0 += 1
    return str(runtime_data.impulses0)


@debug_app.get("/input1")
async def input1():
    """
    ДЛЯ ТЕСТА: Замкнуть Вход 1 (Холодный счетчик)
    """
    runtime_data.impulses1 += 1
    return str(runtime_data.impulses1)


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
