
#include "luaapi.h"

lua_State *LuaState::SetState(lua_State *L)
{
    if (m_L) return m_L;
    m_L = L;
}

void LuaState::Assert(bool b, const char *err)
{
    if (b) return;
    push(err);
    error();
}

void LuaState::CreateLibrary(const char *libname, luaL_Reg l[]) // 0,0
{
    newlib(l);
    setglobal(libname);
}

void LuaState::NewConstantTable(LUA_CONSTANT c[]) // 0,+1
{
    newtable();

    int i = 0;
    while (c[i].name)
    {
        push((lua_Integer) c[i].value);
        setfield(-2, c[i].name);
        i++;
    }
}

void *LuaState::CheckUserdata(int index)
{
    argcheck(isudata(index), index, 0);
    return toudata(index);
}

void *LuaState::NewUserdata(size_t size, const char *name)
{
    void *u = newuserdata(size);
    newmetatable(name);
    setmetatable();
    return u;
}

void **LuaState::NewUserPointer(void *p, const char *tname)
{
    void **pp   = (void **) NewUserdata(sizeof(void *), tname);
    if (pp) *pp = p;
    return pp;
}

void LuaState::NewMetaTable(const char *utname)
{
    if (utname)
        luaL_newmetatable(m_L, utname);
    else
        lua_newtable(m_L);
}

void *LuaState::CheckUserPointer(int index, const char *tname)
{
    void **pp = (void **) checkudata(index, tname);
    return pp ? *pp : (void *) pp;
}

void LuaState::SetMetaTable(const char *utname)
{
    luaL_setmetatable(m_L, utname);
}

void LuaState::SetMetaTable(int nst) { lua_setmetatable(m_L, nst); }

int LuaState::SetMetaField(int nst, const char *field)
{
    if (!getmetatable(nst))
    {
        pop(); // pop the nil
        NewMetaTable();
        SetMetaTable(nst);
        if (!getmetatable(nst))
        {
            pop();
            return 0;
        }
    }
    pushvalue(-2); // copy the value to set
    setfield(-2, field);
    pop(2); // pop the metatable and value to set
    return 1;
}

void *LuaState::ToBuffer(int index)
{
    if (isint(index))
        return (void *) toint(index);
    else if (isstr(index))
        return (void *) tostr(index);
    else if (isudata(index))
        return toudata(index);
    else
        Assert(false, "Can not convert to a buffer");
}

void LuaState::NewLibrary(const char *libname, luaL_Reg l[])
{
    newlib(l);
    pushvalue(-1);
    setglobal(libname);
}

lua_Integer LuaState::OptIntField(int obj, const char *field, lua_Integer b)
{
    getfield(obj, field);
    lua_Integer ret = optinteger(-1, b);
    pop();
    return ret;
}

lua_Number LuaState::OptNumField(int obj, const char *field, lua_Number b)
{
    getfield(obj, field);
    lua_Number ret = optnumber(-1, b);
    pop();
    return ret;
}
const char *LuaState::OptStrField(int obj, const char *field, const char *b)
{
    getfield(obj, field);
    const char *ret = optstring(-1, b);
    pop();
    return ret;
}

void LuaState::CheckFieldType(const char *field, int typ, int t)
{
    getfield(t, field);
    checktype(-1, typ);
    pop(1);
}

bool LuaState::IsFieldType(const char *field, int typ, int t)
{
    getfield(t, field);
    bool ret = (typ == type(-1));
    pop(1);
    return ret;
}

LuaCallback *LuaState::NewCallback(int idcall) // 0,+1
{
    int typ = type(idcall);
    if (!(typ == LUA_TFUNCTION || typ == LUA_TTABLE)) return NULL;
    LuaCallback *l = new LuaCallback(newthread());
    pushvalue(idcall);
    xmove(*this, *l, 1); //回调函数或表
    return l;
}

LuaCallback::LuaCallback(lua_State *L) : LuaState(L)
{
    m_ref = ref(Reg, L); //记录到C注册表防止被回收
}

LuaCallback::~LuaCallback() { RegFreeRef(m_ref); }

int LuaCallback::InitCallback(const char *nf)
{
    settop(1);
    if (nf)
    {
        if (!istable(1)) return 0;
        getfield(1, nf);
        if (isnoneornil(-1)) return 0;
    }
    else
    {
        if (!isfunc(1)) return 0;
        pushvalue(1);
    }
    m_top = gettop();
    return 1;
}

int LuaCallback::InitCallback(int i)
{
    settop(1);
    if (!istable(1)) return 0;
    geti(1, i);
    m_top = gettop();
    return 1;
}

int LuaCallback::BeginCallback(int nr, int msgh)
{
    return pcall(gettop() - m_top, nr, msgh);
}
