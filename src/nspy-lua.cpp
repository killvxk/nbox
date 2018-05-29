
#include <assert.h>

#include "nspy-lua.h"
#include "nbox.h"
#include "nbox-hook.h"
#include "nspy-dbg.h"
#include "dbg-data.h"
#include "nbrpc.h"

NBoxLua g_nblua;

// Lua Hook Handlers in the registry
int g_refluahook;
// Lua Breakpoint Handlers in the registry
int g_refluabps;

using namespace nbox;

static void SetContext(LuaState& lua, CONTEXT *ct)
{
    lua.setglobal("EAX", (lua_Integer)ct->Eax);
    lua.setglobal("EBX", (lua_Integer)ct->Ebx);
    lua.setglobal("ECX", (lua_Integer)ct->Ecx);
    lua.setglobal("EDX", (lua_Integer)ct->Edx);
    lua.setglobal("EBP", (lua_Integer)ct->Ebp);
    lua.setglobal("ESP", (lua_Integer)ct->Esp);
    lua.setglobal("EDI", (lua_Integer)ct->Edi);
    lua.setglobal("ESI", (lua_Integer)ct->Esi);
    lua.setglobal("EFLAGS", (lua_Integer)ct->EFlags);
}

static bool HandleLuaBreakpoint(EXCEPTION_POINTERS *E)
{
    TmpState t(g_nblua);
    t.rget(g_refluabps); assert(!t.isnil());
    {
        // find corresponding handler
        t.get((lua_Integer)E->ExceptionRecord->ExceptionAddress);
        if (t.isnil())
        {
            // find the global handler
            t.getglobal("OnBreakpoint");
            if (t.isnil()) return false;
        }
        SetContext(t, E->ContextRecord);
        // call lua handler
        t.push((lua_Integer)E->ExceptionRecord->ExceptionAddress);
        t.pcall(1, 1);
        return t.isnil();
    }
}

static void HandleLuaHook(void *addr, const HookContext *ht)
{
    TmpState t(g_nblua);
    t.rget(g_refluahook); assert(!t.isnil());
    {
        t.get((lua_Integer)addr);
        assert(t.isfunc());

        CONTEXT ct;
        ct.Eax = ht->EAX;
        ct.Ebx = ht->EBX;
        ct.Ecx = ht->ECX;
        ct.Edx = ht->EDX;
        ct.Ebp = ht->EBP;
        ct.Esp = ht->ESP;
        ct.Edi = ht->EDI;
        ct.Esi = ht->ESI;
        ct.EFlags = ht->EFLAGS;
        SetContext(t, &ct);

        t.push((lua_Integer)addr);
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
    if (addr && readValue<T>(addr, t))
        return lua.ret((lua_Integer)t);
    return lua.ret(false);
}

template<typename T>
static int readFloat(lua_State *L)
{
    LuaState lua = L;
    void *addr = getAddr(lua, 1);
    T t;
    if (addr && readValue<T>(addr, t))
        return lua.ret((lua_Number)t);
    return lua.ret(false);
}

template<typename T>
static int writeInt(lua_State *L)
{
    LuaState lua = L;
    void *addr = getAddr(lua, 1);
    auto t = (T)lua.toint(2);
    return lua.ret(addr && writeValue(addr, t));
}

template<typename T>
static int writeFloat(lua_State *L)
{
    LuaState lua = L;
    void *addr = getAddr(lua, 1);
    auto t = (T)lua.toint(2);
    return lua.ret(addr && writeValue(addr, t));
}

xval::Value Lua2Value(LuaState& lua, int i)
{
    xval::Value v;
    switch (lua.type(i))
    {
    case LUA_TTABLE:
        return "<table>";
    case LUA_TNONE:
    case LUA_TNIL:
        return v;
    case LUA_TBOOLEAN:
        return (bool)lua.toboolean(i);
    case LUA_TSTRING:
    case LUA_TNUMBER:
        return lua.tostr(i);
    default:
        return "<ELSE>";
    }
}

NBoxLua::NBoxLua() : LuaState(LuaState::newstate())
{
    openlibs();

    newtable(); g_refluahook = ref(LuaState::Reg);
    newtable(); g_refluabps = ref(LuaState::Reg);

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

    {
        setglobal("readByte", [](lua_State *L) { return readInt<int8_t>(L); });
        setglobal("readShort", [](lua_State *L) { return readInt<int16_t>(L); });
        auto rint = [](lua_State *L) { return readInt<int32_t>(L); };
        setglobal("readInteger", rint); setglobal("readInt", rint);
        setglobal("readLong", [](lua_State *L) { return readInt<int64_t>(L); });
        setglobal("readFloat", [](lua_State *L) { return readFloat<float>(L); });
        setglobal("readDouble", [](lua_State *L) { return readFloat<double>(L); });
        auto rptr = [](lua_State *L) { return readInt<void*>(L); };
        setglobal("readPointer", rptr); setglobal("readPtr", rptr);
        setglobal("readString", [](lua_State *L)
        {
            LuaState lua = L;
            auto addr = getAddr(lua, 1);
            auto size = lua.toint(2);
            if (addr)
                return lua.ret(size ? readBytes(addr, size) : readString(addr));
            else
                return lua.ret(false);
        });
    }

    {
        setglobal("writeByte", [](lua_State *L) { return writeInt<int8_t>(L); });
        setglobal("writeShort", [](lua_State *L) { return writeInt<int16_t>(L); });
        auto wint = [](lua_State *L) { return writeInt<int32_t>(L); };
        setglobal("writeInt", wint); setglobal("writeInteger", wint);
        setglobal("writeLong", [](lua_State *L) { return writeInt<int64_t>(L); });
        setglobal("writeFloat", [](lua_State *L) { return writeFloat<float>(L); });
        setglobal("writeDouble", [](lua_State *L) { return writeFloat<double>(L); });
        auto wptr = [](lua_State *L) { return writeInt<void*>(L); };
        setglobal("writePointer", wptr); setglobal("writePtr", wptr);
    }

    setglobal("setHook", [](lua_State *L)
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

    setglobal("removeHook", [](lua_State *L)
    {
        LuaState lua = L;
        void *addr = getAddr(lua, 1);
        if (!addr) return lua.ret(false);
        lua.rget(g_refluahook);
        {
            delhook(addr);
            lua.set((lua_Integer)addr, LuaState::Nil);
            return lua.ret(true);
        }
    });

    setglobal("addBreakpoint", [](lua_State *L)
    {
        LuaState lua = L;
        void *addr = getAddr(lua, 1);
        if (!addr) return lua.ret(false);
        auto bp = Breakpoint::Add(addr, HandleLuaBreakpoint);
        if (!bp) return lua.ret(false);
        if (lua.isfunc(2))
        {
            lua.rget(g_refluabps);
            lua.set((lua_Integer)addr, LuaState::i2);
        }
        return lua.ret(true);
    });

    setglobal("removeBreakpoint", [](lua_State *L)
    {
        LuaState lua = L;
        void *addr = getAddr(lua, 1);
        auto bp = Breakpoint::Get(addr);
        if (bp) bp->remove();
        return 0;
    });

    // get breakpoint

    setglobal("LOG", [](lua_State *L)
    {
        LuaState lua = L;
        auto argc = lua.argc();
        if (!argc)
            return 0;
        auto v = xval::List::New(argc);
        for (int i = 1; i <= argc; ++i)
            v.list().push_back(Lua2Value(lua, i));
        g_ses.fnotify("ndbg#print", v.list());
        return 0;
    });

    setglobal("TEST", [](lua_State *L)
    {
        MessageBox(nullptr, "AAAAAAA", "BBBBBBBB", 0);
        return 0;
    });
}
