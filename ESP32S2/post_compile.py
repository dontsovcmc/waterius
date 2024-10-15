import shutil
import os
Import("env")


def post_build(source, target, env):
    v = ''.join(c for c in env.GetProjectOption("firmware_version") if c.isdigit() or c in ['.'])
    board = env.GetProjectOption("board")
    bin = target[0].get_abspath()
    dest = os.path.join(os.path.pardir, os.path.pardir, env["PROJECT_DIR"], board + '-' + v + '.bin')
    print(bin + ' ->\n' + dest)
    shutil.copy(bin, dest)

    elf = source[0].get_abspath()
    dest = os.path.join(os.path.pardir, os.path.pardir, env["PROJECT_DIR"], board + '-' + v + '.elf')
    print(elf + ' ->\n' + dest)
    shutil.copy(elf, dest)

env.AddPostAction("$BUILD_DIR/firmware.bin", post_build)


def post_buildfs(source, target, env):
    v = ''.join(c for c in env.GetProjectOption("firmware_version") if c.isdigit() or c in ['.'])
    board = env.GetProjectOption("board")
    print('Move BIN files of filesystem version=' + v)
    bin = target[0].get_abspath()
    dest = os.path.join(os.path.pardir, os.path.pardir, env["PROJECT_DIR"], board + '-' + v + '-fs.bin')
    print(bin + ' ->\n' + dest)
    shutil.copy(bin, dest)

env.AddPostAction("$BUILD_DIR/littlefs.bin", post_buildfs)
