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
    make_json(file_path, dest, env)


env.AddPostAction("$BUILD_DIR/firmware.bin", lambda source, target, env: copy_file(source, target, env))
env.AddPostAction("$BUILD_DIR/firmware.elf", lambda source, target, env: copy_file(source, target, env))
env.AddPostAction("$BUILD_DIR/littlefs.bin", lambda source, target, env: copy_file(source, target, env, '-fs'))
