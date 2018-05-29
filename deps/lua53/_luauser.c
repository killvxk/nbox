
#include "lua.h"
#include "_luauser.h"
#include <windows.h>

static struct
{
    CRITICAL_SECTION LockSct;
    BOOL             Init;
} Gl;

extern int luaopen_systhread(lua_State *L);

void LuaLockInitial(lua_State *L)
{
    if (!Gl.Init)
    {
        /* Create a mutex */
        InitializeCriticalSection(&Gl.LockSct);
        Gl.Init = TRUE;
        luaopen_systhread(L);
    }
}

void LuaLockFinal(lua_State *L) /* Not called by Lua. */
{
    /* Destroy a mutex. */
    if (Gl.Init)
    {
        DeleteCriticalSection(&Gl.LockSct);
        Gl.Init = FALSE;
    }
}

void LuaLock(lua_State *L)
{
    /* Wait for control of mutex */
    EnterCriticalSection(&Gl.LockSct);
}

void LuaUnlock(lua_State *L)
{
    /* Release control of mutex */
    LeaveCriticalSection(&Gl.LockSct);
}
