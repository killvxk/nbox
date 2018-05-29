
target 'lua53'
    set_kind 'shared'

    add_files('*.c|lua.c|luac.c')
    add_defines('LUA_BUILD_AS_DLL')
    add_cxflags [[LUA_USER_H=\"_luauser.h\"]]
