# Copyright 2025 Nick Brassel (@tzarc)
# SPDX-License-Identifier: GPL-2.0-or-later

all: test

.PHONY: generated-files all test clean
generated-files: keycode_lookup.c

keycode_lookup: generated-files
	@gcc -DKEYCODE_TESTS -o keycode_lookup keycode_lookup.c

test: keycode_lookup
	@./keycode_lookup

clean:
	@rm -f keycode_lookup keycode_lookup.c

keycode_lookup.c: make_keycode_lookup.py keycode_lookup.c.j2
	@python3 make_keycode_lookup.py > keycode_lookup.c

OPT_DEFS += -DMAKE_LIB
VPATH += ../lib/lua
LUA_SRC += \
    ../lib/lua/lzio.c \
    ../lib/lua/lctype.c \
    ../lib/lua/lopcodes.c \
    ../lib/lua/lmem.c \
    ../lib/lua/lundump.c \
    ../lib/lua/ldump.c \
    ../lib/lua/lstate.c \
    ../lib/lua/lgc.c \
    ../lib/lua/llex.c \
    ../lib/lua/lcode.c \
    ../lib/lua/lparser.c \
    ../lib/lua/ldebug.c \
    ../lib/lua/lfunc.c \
    ../lib/lua/lobject.c \
    ../lib/lua/ltm.c \
    ../lib/lua/lstring.c \
    ../lib/lua/ltable.c \
    ../lib/lua/ldo.c \
    ../lib/lua/lvm.c \
    ../lib/lua/lapi.c \
    ../lib/lua/lauxlib.c \
    ../lib/lua/lbaselib.c \
    ../lib/lua/lcorolib.c \
    ../lib/lua/lmathlib.c \
    ../lib/lua/lstrlib.c \
    ../lib/lua/ltablib.c \
    ../lib/lua/lutf8lib.c \
    ../lib/lua/loadlib.c \
    ../lib/lua/liolib.c \
    ../lib/lua/loslib.c \
    ../lib/lua/ldblib.c \
    ../lib/lua/linit.c

test_app: keycode_lookup
	@gcc -DMAKE_LIB -DKEYCODE_LOOKUP_MEMOISE -o test_app test_app.c $(LUA_SRC) -include config.h -I../lib/lua -lc -lm -flto=auto
