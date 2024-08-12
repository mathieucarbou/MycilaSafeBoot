# SPDX-License-Identifier: MIT
#
# Copyright (C) 2023-2024 Mathieu Carbou
#
Import("env")
import os


def safeboot(source, target, env):
    # platformio estimates the amount of flash used to store the firmware. this
    # estimate is not accurate. we perform a final check on the firmware bin
    # size by comparing it against the respective partition size.
    max_size = env.BoardConfig().get("upload.maximum_size", 1)
    fw_size = os.path.getsize(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    if fw_size > max_size:
        raise Exception("firmware binary too large: %d > %d" % (fw_size, max_size))
    
    print("Firmware size valid: %d <= %d" % (fw_size, max_size))

    os.rename(
        env.subst("$BUILD_DIR/${PROGNAME}.bin"), env.subst("$BUILD_DIR/safeboot.bin")
    )

    print("SafeBoot firmware created: %s" % env.subst("$BUILD_DIR/safeboot.bin"))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", safeboot)
