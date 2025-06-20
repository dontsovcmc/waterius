import shutil
import os
Import("env", "projenv")


def prefix(env):
    cpp_defines = env.get("CPPDEFINES", [])
    FIRMWARE_VER = 0
    WATERIUS_MODEL = 0
    for define in cpp_defines:
        if isinstance(define, tuple):
            if define[0] == "FIRMWARE_VER":
                FIRMWARE_VER = define[1]
            if define[0] == "WATERIUS_MODEL":
                WATERIUS_MODEL = define[1]
            
    board = env.GetProjectOption("board")
    if WATERIUS_MODEL == 0:
    	return f"{board}-{FIRMWARE_VER}"
    return f"{board}-{FIRMWARE_VER}-{WATERIUS_MODEL}"


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

