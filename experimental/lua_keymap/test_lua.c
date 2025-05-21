// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

lua_State *L = 0;

void luaL_openlibs_custom(lua_State *L) {
    // clang-format off
    static const luaL_Reg loadedlibs[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_COLIBNAME, luaopen_coroutine},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {NULL, NULL},
    };
    // clang-format on

    const luaL_Reg *lib;
    /* "require" functions from 'loadedlibs' and set results to global table */
    for (lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); /* remove lib */
    }
}

#if 0
static int nil_returner(lua_State *L) {
    lua_pushnil(L);
    return 1;
}

static int true_returner(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

static int false_returner(lua_State *L) {
    lua_pushboolean(L, false);
    return 1;
}
#endif

#include "keycode_lookup.c"
static int keycode_lookup_indexer(lua_State *L) {
    int n = lua_gettop(L); // number of arguments
    if (n != 2) {
        luaL_error(L, "keycode_lookup_indexer: expected 2 arguments, got %d", n);
        return 0;
    }

    if (!lua_istable(L, 1)) {
        luaL_error(L, "keycode_lookup_indexer: expected table as first argument");
        return 0;
    }
    if (!lua_isstring(L, 2)) {
        lua_pushnil(L);
        return 1;
    }

    const char *name = luaL_checkstring(L, 2); // second arg is what we want to print

    int      iterations = 0;
    uint16_t value      = lookup_keycode_by_name(name, &iterations);
    if (value != 0 || strcmp(name, "KC_NO") == 0) {
        printf("keycode_lookup_indexer: %s -> 0x%04X (%d iterations)\n", name, value, iterations);

#ifdef KEYCODE_LOOKUP_MEMOISE
        // Memoise the value in the table
        lua_pushinteger(L, value);
        lua_rawset(L, 1);
#endif // KEYCODE_LOOKUP_MEMOISE

        // Return the value
        lua_pushinteger(L, value);
        return 1;
    }

    // Nothing found, return nil
    lua_pushnil(L);
    return 1;
}

void test_lua(void) {
    L = luaL_newstate();
    luaL_openlibs_custom(L);

    // Set up a metatable on _G
    {
        lua_pushglobaltable(L);
        // create a new table and set it as the metatable for the global table if it doesn't already exist (which _should_ be the case)
        if (!lua_getmetatable(L, 1)) {
            lua_newtable(L);
            lua_setmetatable(L, 1);
        }
        lua_pop(L, 1); // pop the global table
    }

    // Do the equivalent of getmetatable(_G).__index = keycode_lookup_indexer
    {
        lua_pushglobaltable(L);                        // push the global table
        lua_getmetatable(L, -1);                       // get the metatable
        lua_pushcfunction(L, &keycode_lookup_indexer); // push the keycode_lookup function
        lua_setfield(L, -2, "__index");                // set the __index field to the keycode_lookup_indexer function
        lua_pop(L, 2);                                 // pop the metatable, global table
    }

    const char *code = "print(string.format('UG_VALU = 0x%04X', UG_VALU))\nprint(string.format('KC_NO = 0x%04X', KC_NO))\nprint(string.format('UG_VALU = 0x%04X', UG_VALU))";
    if (luaL_loadstring(L, code) == LUA_OK) {
        if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
            lua_pop(L, lua_gettop(L));
        } else {
            printf("Failed lua_pcall\n");
        }
    } else {
        printf("Failed luaL_loadstring\n");
    }

    lua_close(L);
}
