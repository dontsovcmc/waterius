import shutil
import os
import hashlib
import json
Import("env")


def getCppDefine(name: str) -> str:
    cpp_defines = env.get("CPPDEFINES", [])
    for define in cpp_defines:
        if isinstance(define, tuple):
            if define[0] == name:
                return define[1]
    return None


def prefix(env):
    v = ''.join(c for c in env.GetProjectOption("firmware_version") if c.isdigit() or c in ['.'])
    env_name = env["PIOENV"]
    # board = env.GetProjectOption("board")
    
    if getCppDefine("WIFI_SSID"):
        return f"{env_name}-{v}-test"
    return f"{env_name}-{v}"


def make_json(file_path, dest, env):
    with open(file_path, 'rb') as f:
        md5 = hashlib.md5(f.read()).hexdigest()
    version = ''.join(c for c in env.GetProjectOption("firmware_version") if c.isdigit() or c in ['.'])
    info = {
        "filename": os.path.basename(dest),
        "md5": md5,
        "version": version
    }
    json_path = os.path.splitext(dest)[0] + '.json'
    with open(json_path, 'w') as f:
        json.dump(info, f, indent=2)
    print(f'JSON -> {json_path}')


def copy_file(source, target, env, postfix=''):
    file_path = target[0].get_abspath()
    ext = file_path.split('.')[-1]
    dest = os.path.join(os.path.pardir,
                        os.path.pardir,
                        os.path.pardir,
                        env["PROJECT_DIR"], f'{prefix(env)}{postfix}.{ext}')
    print(file_path + ' ->\n' + dest)
    shutil.copy(file_path, dest)
    #make_json(file_path, dest, env)


def merge_full(source, target, env):
    """Единый образ для прошивки с адреса 0x0 (web.esphome.io / ESP Web Tools):
    firmware.bin @ 0x0 + образ ФС @ FS_START, промежуток заполнен 0xFF.

    Это эквивалент `esptool merge_bin` для ESP8266, но без зависимости от
    esptool v4 (платформа espressif8266@4.2.1 пинит esptool v3.0 без merge_bin).
    Вешается на сборку ФС, т.к. нужны оба образа (firmware.bin собирается
    отдельной целью `pio run`, ФС — `pio run -t buildfs`)."""
    build_dir = env.subst("$BUILD_DIR")
    fw = os.path.join(build_dir, "firmware.bin")
    fs_type = env.GetProjectOption("board_build.filesystem", "littlefs")
    fs = os.path.join(build_dir, f"{fs_type}.bin")

    if not os.path.isfile(fw):
        print(">> [merge] нет firmware.bin — сначала: pio run")
        return

    fs_offset = int(env["FS_START"])  # смещение ФС во flash (4m1m -> 0x300000)

    with open(fw, "rb") as f:
        image = bytearray(f.read())
    if len(image) > fs_offset:
        print(f">> [merge] firmware ({len(image)} б) не помещается до ФС ({hex(fs_offset)})")
        return
    image += b"\xFF" * (fs_offset - len(image))
    with open(fs, "rb") as f:
        image += f.read()

    dest = os.path.join(env.subst("$PROJECT_DIR"), f"{prefix(env)}-full.bin")
    with open(dest, "wb") as f:
        f.write(image)
    print(f">> [merge] единый образ (ФС @ {hex(fs_offset)}, {len(image)} б): {dest}")


env.AddPostAction("$BUILD_DIR/firmware.bin", lambda source, target, env: copy_file(source, target, env))
env.AddPostAction("$BUILD_DIR/firmware.elf", lambda source, target, env: copy_file(source, target, env))
env.AddPostAction("$BUILD_DIR/littlefs.bin", lambda source, target, env: copy_file(source, target, env, '-fs'))
env.AddPostAction("$BUILD_DIR/littlefs.bin", merge_full)
