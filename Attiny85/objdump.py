import shutil
import os
Import("env", "projenv")


def getCppDefine(name: str) -> str:
    cpp_defines = env.get("CPPDEFINES", [])
    for define in cpp_defines:
        if isinstance(define, tuple):
            if define[0] == name:
                return define[1]
        elif isinstance(define, str):
            if define == name:
                return True
    return None


def prefix(env):
    FIRMWARE_VER = getCppDefine("FIRMWARE_VER")
    WATERIUS_MODEL = getCppDefine("WATERIUS_MODEL")
    LOG_ON = getCppDefine("LOG_ON")
            
    board = env.GetProjectOption("board")
    out = f"{board}-{FIRMWARE_VER}"
    if WATERIUS_MODEL and WATERIUS_MODEL > 0:
        out = f"{out}-{WATERIUS_MODEL}"
    if LOG_ON:
        out = f"{out}-log"
    return out


def copy_file(source, target, env, postfix=''):
    
    file_path = target[0].get_abspath()
    ext = file_path.split('.')[-1]
    dest = os.path.join(os.path.pardir, 
                        os.path.pardir,
                        env["PROJECT_DIR"], f'{prefix(env)}{postfix}.{ext}')
    print(file_path + ' ->\n' + dest)
    shutil.copy(file_path, dest)
    

env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", lambda source, target, env: copy_file(source, target, env))

env.AddPostAction(
	"$BUILD_DIR/${PROGNAME}.elf",
	env.VerboseAction("avr-objdump -d $BUILD_DIR/${PROGNAME}.elf > $BUILD_DIR/${PROGNAME}.lss", "Creating $BUILD_DIR/${PROGNAME}.lss")
)

