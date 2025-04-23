# Copyright 2025 Nick Brassel (@tzarc)
# SPDX-License-Identifier: GPL-2.0-or-later
VPATH += $(MODULE_PATH_LUA_KEYMAP)/lib/lua

OPT_DEFS += -DMAKE_LIB
SRC += \
    lzio.c \
    lctype.c \
    lopcodes.c \
    lmem.c \
    lundump.c \
    ldump.c \
    lstate.c \
    lgc.c \
    llex.c \
    lcode.c \
    lparser.c \
    ldebug.c \
    lfunc.c \
    lobject.c \
    ltm.c \
    lstring.c \
    ltable.c \
    ldo.c \
    lvm.c \
    lapi.c \
    lauxlib.c \
    lbaselib.c \
    lcorolib.c \
    lmathlib.c \
    lstrlib.c \
    ltablib.c \
    lutf8lib.c \
    linit.c

SRC += test_lua.c
