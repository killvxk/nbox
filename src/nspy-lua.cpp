
#include <assert.h>

#include "nspy-lua.h"
#include "nbox.h"
#include "nbox-hook.h"
#include "nspy-dbg.h"
#include "dbg-data.h"

NBoxLua g_nblua;

// Lua Hook Handlers in the registry
int g_refluahook;
// Lua Breakpoint Handlers in the registry
int g_refluabps;

using namespace nbox;

static bool HandleLuaBreakpoint(EXCEPTION_POINTERS *E)
{
    auto t = g_nblua.newthread();
    t.rget(g_refluahook); assert(!t.isnil());
    {
        t.get((lua_Integer)E->ExceptionRecord->ExceptionAddress);
        if (t.isnil())
            return DS_NOTHANDLE;
        // call lua handler
    }
}

static void HandleLuaHook(void *addr, const HookContext *ct)
{
    auto t = g_nblua.newthread();
    t.rget(g_refluahook); assert(!t.isnil());
    {
        t.get((lua_Integer)addr);
        assert(!t.isnil());

        t.newtable(0, 9);
        t.set("EAX", ct->EAX);
        t.set("EBX", ct->EBX);
        t.set("ECX", ct->ECX);
        t.set("EDX", ct->EDX);
        t.set("EBP", ct->EBP);
        t.set("ESP", ct->ESP);
        t.set("EDI", ct->EDI);
        t.set("ESI", ct->ESI);
        t.set("EFLAGS", ct->EFLAGS);

        t.pcall(1, 0);
    }
}

inline void *getAddr(LuaState& L, int i)
{
    return L.iststr(1) ? getAddress(L.tostr(1)) : reinterpret_cast<void*>(L.toint(1));
}

template<typename T>
static int readInt(lua_State *L)
{
    LuaState lua = L;
    void *addr = getAddr(lua, 1);
    T t;
    if (addr && readValue<T>((uint8_t*)addr, t))
        return lua.ret((lua_Integer)t);
    else
        return lua.ret(false);
}

NBoxLua::NBoxLua() : LuaState(LuaState::newstate())
{
    openlibs();

    newtable(); g_refluahook = rref();
    newtable(); g_refluabps = rref();

    setglobal("getAddress", [](lua_State *l)
    {
        LuaState L = l;
        if (L.iststr(1))
            return L.ret((lua_Integer)getAddress(L.tostr(1)));
        return L.ret(false);
    });

    setglobal("getSymbol", [](lua_State *l)
    {
        LuaState L = l;
        return L.ret(getSymbol((void*)L.optinteger(1, 0)));
    });

    setglobal("readString", [](lua_State *L)
    {
        LuaState lua = L;
        void *addr = getAddr(lua, 1);
        return addr ? lua.ret(readString((uint8_t*)addr)) : lua.ret(false);
    });

    setglobal("readBytes", [](lua_State *L)
    {
        LuaState lua = L;
        void *addr = getAddr(lua, 1);
        if (addr && lua.isint(2))
            return lua.ret(readBytes((uint8_t*)addr, lua.toint(2)));
        else
            return lua.ret("");
    });

    setglobal("readByte", [](lua_State *L) { return readInt<int8_t>(L); });
    setglobal("readShort", [](lua_State *L) { return readInt<int16_t>(L); });
    setglobal("readInteger", [](lua_State *L) { return readInt<int32_t>(L); });
    setglobal("readLong", [](lua_State *L) { return readInt<int64_t>(L); });
    setglobal("readPointer", [](lua_State *L) { return readInt<void*>(L); });

    setglobal("sethook", [](lua_State *L)
    {
        LuaState lua = L;

        void *addr = getAddr(lua, 1);

        if (addr && lua.isfunc(2))
        {
            lua.rget(g_refluahook);
            {
                lua.get((lua_Integer)addr);
                // no this addr in table, set hook success
                if (lua.isnil() && !sethook(addr, HandleLuaHook))
                {
                    lua.pop();
                    // record to table
                    lua.set((lua_Integer)addr, Index(2));
                    return lua.ret(true);
                }
            }
        }
        return lua.ret(false);
    });

    setglobal("TEST", [](lua_State *L)
    {
        MessageBox(nullptr, "AAAAAAA", "BBBBBBBB", 0);
        MessageBox(nullptr, "CCCCCCC", "BBBBBBBB", 0);
        MessageBox(nullptr, "DDDDDDD", "BBBBBBBB", 0);
        return 0;
    });
}
