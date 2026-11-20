# Заводской образ без сети и email не пройдёт тест: без secrets.ini сборка падает.
# Значения не печатаем — только имена пустых ключей.
Import("env")

config = env.GetProjectConfig()
empty = [key for key in ("wifi_ssid", "wifi_password", "waterius_email")
         if not config.get("secrets", key, "").strip()]
if empty:
    print("secrets.ini: пусто в " + ", ".join(empty) + " — заводской образ не собран")
    env.Exit(1)
