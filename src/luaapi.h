#ifndef __LUAAPI_H__
#define __LUAAPI_H__

#include <Windows.h>
#include <stdarg.h>
#include <string>

#include <lua53/lua.hpp>

#define LUA_END_FIELD                                                          \
    {                                                                          \
        0, 0                                                                   \
    }

class LuaCallback;

class LuaState
{
public:
    LuaState(lua_State *L) { m_L = L; }
    // LuaState(const LuaState& other) = delete;
    LuaState(LuaState&& other)
    {
        m_L = other.m_L;
        other.m_L = nullptr;
    }
    ~LuaState() {}

    lua_State *SetState(lua_State *L); //如果存在则返回原来的lua_State

    typedef struct
    {
        const char *name;
        int         value;
    } LUA_CONSTANT;

    enum Index
    {
        i0 = 0, i1, i2, i3, i4, i5, i6, i7, i8,
        Reg = LUA_REGISTRYINDEX,
    };
    enum NIL { Nil };

    // push...
    void pushvalue(int i) { lua_pushvalue(m_L, i); }
    void pushnumber(lua_Number n) { lua_pushnumber(m_L, n); }
    void pushboolean(int b) { lua_pushboolean(m_L, b); }
    void pushinteger(lua_Integer i) { lua_pushinteger(m_L, i); }
    int  pushthread(lua_State *L) { return lua_pushthread(L); }
    void pushglobaltable() { lua_pushglobaltable(m_L); }
    void pushlightuserdata(void *p) { lua_pushlightuserdata(m_L, p); }

    void push(lua_Integer i) { lua_pushinteger(m_L, i); }
    void push(size_t i) { lua_pushinteger(m_L, i); }
    void push(int i) { lua_pushinteger(m_L, i); }
    void push(long i) { lua_pushinteger(m_L, i); }
    void push(unsigned long long i) { lua_pushinteger(m_L, i); }
    void push(void *p) { lua_pushlightuserdata(m_L, p); }
    void push(lua_Number n) { lua_pushnumber(m_L, n); }
    void push(bool b) { lua_pushboolean(m_L, b); }
    void push(lua_CFunction f, int n = 0) { lua_pushcclosure(m_L, f, n); }
    void push(Index i) { lua_pushvalue(m_L, (int)i); }
    void push(NIL _) { lua_pushnil(m_L); }

    void pushnil(void) { lua_pushnil(m_L); }
    const char *push(const char *s) { return lua_pushstring(m_L, s); }
    const char *push(const char *s, size_t len)
    {
        return lua_pushlstring(m_L, s, len);
    }
    const char *push(const std::string& str) { return push(str.c_str(), str.size()); }

    template<typename T, typename... Args>
    void pushx(T arg1, Args... rest)
    {
        push(arg1); pushx(rest...);
    }

    const char *pushvfstring(const char *fmt, va_list args)
    {
        return lua_pushvfstring(m_L, fmt, args);
    }
    const char *pushfstring(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        return lua_pushvfstring(m_L, fmt, args);
    }
    // to...
    int toboolean(int i) { return lua_toboolean(m_L, i); }
    lua_CFunction tocfunction(int i)
    {
        return lua_tocfunction(m_L, i);
    }
    lua_Integer toint(int i) { return lua_tointeger(m_L, i); }
    lua_Integer tointx(int i, int *isnum = NULL)
    {
        return lua_tointegerx(m_L, i, isnum);
    }
    const char *tolstring(int i, size_t *len) { return lua_tolstring(m_L, i, len); }
    const char *tostr(int i = -1) { return lua_tostring(m_L, i); }
    lua_Number tonum(int i = -1) { return lua_tonumber(m_L, i); }
    lua_Number tonumx(int i = -1, int *isnum = NULL)
    {
        return lua_tonumberx(m_L, i, isnum);
    }
    void *toudata(int i = -1) { return lua_touserdata(m_L, i); };
    lua_State *tothread(int i = -1) { return lua_tothread(m_L, i); };

    // LUA_OK LUA_YIELD
    int status() { return lua_status(m_L); }
    // opt..
    lua_Integer optinteger(int arg, lua_Integer d) { return luaL_optinteger(m_L, arg, d); }
    const char *optlstring(int arg, const char *d, size_t *l = NULL)
    {
        return luaL_optlstring(m_L, arg, d, l);
    }
    lua_Number optnumber(int arg, lua_Number d) { return luaL_optinteger(m_L, arg, d); }
    const char *optstring(int arg, const char *d) { return luaL_optstring(m_L, arg, d); }
    // load ...
    int dostring(const char *str) { return luaL_dostring(m_L, str); }
    int dofile(const char *file) { return luaL_dofile(m_L, file); }
    int loadfile(const char *file) { return luaL_loadfile(m_L, file); }
    int loadbuffer(const char *buf, size_t len, const char *name)
    {
        return luaL_loadbuffer(m_L, buf, len, name);
    }
    int pcall(int nargs, int nresults, int msgh = 0)
    {
        return lua_pcall(m_L, nargs, nresults, msgh);
    }
    // check ...
    int checkstack(int n) { return lua_checkstack(m_L, n); }
    void checkstack(int sz, const char *msg = NULL)
    {
        luaL_checkstack(m_L, sz, msg);
    }

    void checktype(int arg, int t)
    {
        luaL_checktype(m_L, arg, t);
    }
    void *checkudata(int ud, const char *tname)
    {
        return luaL_checkudata(m_L, ud, tname);
    }
    const char *checkstring(int arg)
    {
        return luaL_checkstring(m_L, arg);
    }
    int checkoption(int arg, const char *def, const char *lst[])
    {
        return luaL_checkoption(m_L, arg, def, lst);
    }
    lua_Integer checkinteger(int arg)
    {
        return luaL_checkinteger(m_L, arg);
    }

    void argcheck(int cond, int iarg, const char *msg)
    {
        luaL_argcheck(m_L, cond, iarg, msg);
    }
    int argerror(int iarg, const char *msg)
    {
        return luaL_argerror(m_L, iarg, msg);
    }

    // create ...
    void createtable(int narr, int nrec)
    {
        lua_createtable(m_L, narr, nrec);
    }

    // is ...
    int isnil(int i = -1) { return lua_isnil(m_L, i); }
    int isnone(int i = -1) { return lua_isnone(m_L, i); }
    int isnoneornil(int i = -1) { return lua_isnoneornil(m_L, i); }
    int isfunc(int i = -1) { return lua_isfunction(m_L, i); }
    int iscfunc(int i = -1) { return lua_iscfunction(m_L, i); }
    int isnum(int i = -1) { return lua_isnumber(m_L, i); }      // !!! isnum or number string
    int isint(int i = -1) { return lua_isinteger(m_L, i); }
    int isudata(int i = -1) { return lua_isuserdata(m_L, i); }
    int isludata(int i = -1) { return lua_islightuserdata(m_L, i); }
    int isstr(int i = -1) { return lua_isstring(m_L, i); }      // !!! is string or number
    int istable(int i = -1) { return lua_istable(m_L, i); }
    int isthread(int i = -1) { return lua_isthread(m_L, i); }
    int isbool(int i = -1) { return lua_isboolean(m_L, i); }

    bool iststr(int i = -1) { return lua_type(m_L, i) == LUA_TSTRING; }
    bool istnum(int i = -1) { return lua_type(m_L, i) == LUA_TNUMBER; }

    // 0, +1
    void *newuserdata(size_t size) { return lua_newuserdata(m_L, size); }
    template<class T>
    T *newuserdata(T& t) { return (T*)lua_newuserdata(m_L, sizeof(T)); }
    void newtable(int narr = 0, int nrec = 0) { lua_createtable(m_L, narr, nrec); }

    int newmetatable(const char *tname)
    {
        return luaL_newmetatable(m_L, tname);
    }
    void newlib(const luaL_Reg l[]) { return luaL_newlib(m_L, l); }
    void newlibtable(const luaL_Reg l[])
    {
        return luaL_newlibtable(m_L, l);
    }
    // 0, +1
    lua_State *newthread() { return lua_newthread(m_L); }

    // -1, 0
    void setfield(int i, const char *k) { lua_setfield(m_L, i, k); }
    void seti(int i, lua_Integer n) { lua_seti(m_L, i, n); }
    void setglobal(const char *name) { return lua_setglobal(m_L, name); }

    template<typename T>
    void set(const char *k, T t) { push(t); setfield(-2, k); }
    template<typename T>
    void set(lua_Integer k, T t) { push(t); seti(-2, k); }
    template<typename T>
    void set(Index k, T t) { push(k), push(t); settable(-3); }

    int getfield(int i, const char *k) { return lua_getfield(m_L, i, k); }
    int geti(int i, lua_Integer n) { return lua_geti(m_L, i, n); }
    int getglobal(const char *name) { return lua_getglobal(m_L, name); }

    template<typename T>
    void get(T t) { push(t); gettable(-2); }

    // -2, 0
    void settable(int i) { lua_settable(m_L, i); }

    // -1, +1
    int gettable(int i = -2) { return lua_gettable(m_L, i); }
    int setmetatable(int i = -2)
    {
        return lua_setmetatable(m_L, i);
    }
    int getmetatable(int i = -1)
    {
        return lua_getmetatable(m_L, i);
    }

    template <typename T>
    void setglobal(const char *name, T v)
    {
        push(v); setglobal(name);
    }

    void setuservalue(int i = -2)
    {
        return lua_setuservalue(m_L, i);
    }
    int getuservalue(int i = -1)
    {
        return lua_getuservalue(m_L, i);
    }
    void setfuncs(const luaL_Reg *l, int nup = 0)
    {
        return luaL_setfuncs(m_L, l, nup);
    }

    int gettop() { return lua_gettop(m_L); }
    int argc() { return gettop(); }

    void settop(int i) { return lua_settop(m_L, i); }
    int getsubtable(int idx, const char *fname)
    {
        return luaL_getsubtable(m_L, idx, fname);
    }

    int resume(lua_State *from, int nargs)
    {
        return lua_resume(m_L, from, nargs);
    }
    int yield(lua_State *L, int nargs) { return lua_yield(L, nargs); }

    void traceback(lua_State *L, const char *msg = nullptr, int level = 1)
    {
        luaL_traceback(m_L, L, msg, level);
    }

    LuaState& pop(int n = 1) { lua_pop(m_L, n); return *this; }
    int type(int i = -1) { return lua_type(m_L, i); }
    const char *type_name(int tp) { return lua_typename(m_L, tp); }

    // -1, +0
    int ref(int t) { return luaL_ref(m_L, t); }
    template<typename T>
    int ref(int it, T t) { return push(t), ref(it); }

    // 0, 0
    void unref(int t, int ref) { return luaL_unref(m_L, t, ref); }

    void rawseti(int t, int n) { return lua_rawseti(m_L, t, n); }
    int rawgeti(int t, int n) { return lua_rawgeti(m_L, t, n); }
    void rawset(int t) { return lua_rawset(m_L, t); }
    int rawget(int t) { return lua_rawget(m_L, t); }
    int rawlen(int t) { return lua_rawlen(m_L, t); }
    int error() { return lua_error(m_L); }
    void openlibs() { return luaL_openlibs(m_L); }
    void close() { lua_close(m_L); }

    int callmeta(int obj, const char *e)
    {
        return luaL_callmeta(m_L, obj, e);
    }
    void *testudata(int arg, const char *tname)
    {
        return luaL_testudata(m_L, arg, tname);
    }

    operator lua_State *() { return m_L; }

    void *      ToBuffer(int);
    int         SetMetaField(int, const char *); //-1, 0
    void SetMetaTable(const char *);      //-1, 0
    void SetMetaTable(int);               //-1, 0
    void Assert(bool b, const char *err);
    void CreateLibrary(const char *libname, luaL_Reg l[]); // 0,0
    void *CheckUserdata(int i);
    void NewLibrary(const char *libname, luaL_Reg l[]); // 0,+1
    void NewConstantTable(LUA_CONSTANT c[]);            // 0,+1
    void NewMetaTable(const char *utname = NULL);       // 0,+1
    void *NewUserdata(size_t,
                      const char *name); // 0,+1 新建一个类型为name的userdata
    void **NewUserPointer(void *p, const char *);    // 0,+1
    void *CheckUserPointer(int i, const char *); // 0,0
    void CheckFieldType(const char *field, int typ, int t = -1);
    bool IsFieldType(const char *field, int typ, int t = -1);
    LuaCallback *NewCallback(int); // 0,+1

    // OptField         0,0
    lua_Integer OptIntField(int obj, const char *field, lua_Integer b);
    lua_Number OptNumField(int obj, const char *field, lua_Number b);
    const char *OptStrField(int obj, const char *field, const char *b);

    // C Registry table
    // -1, 0
    //void rsetfield(const char *gname) { setfield(LUA_REGISTRYINDEX, gname); }
    //int rref() { return ref(LUA_REGISTRYINDEX); }
    //template<typename T>
    //int rref(T t) { return push(t), ref(LUA_REGISTRYINDEX); }

    // 0, +1
    int rget(int ref) { return rawgeti(LUA_REGISTRYINDEX, ref); }
    int rget(const char *field) { return getfield(LUA_REGISTRYINDEX, field); }

    void RegFreeRef(int ref) { return unref(LUA_REGISTRYINDEX, ref); }

    template<typename T>
    int ret(T t) { return push(t), 1; }

    template<typename T, typename... Args>
    int ret(T t, Args... args) { return push(t), ret(args...) + 1; }

    // void   Lock(){ m_locked = true; }
    // void   Unlock(){ m_locked = false; }
    // bool   IsLocked(){ return m_locked; }

protected:
    lua_State *m_L = nullptr;

    // Static function
public:
    static lua_State *newstate() { return luaL_newstate(); }
    static lua_State *newstate(lua_Alloc f, void *ud)
    {
        return lua_newstate(f, ud);
    }

    static void xmove(lua_State *from, lua_State *to, int n = 1)
    {
        return lua_xmove(from, to, n);
    }
};

class TmpState : public LuaState
{
public:
    TmpState(LuaState& G) : LuaState(G.newthread())
    {
        _ref = G.ref(Reg);
        openlibs();
    }
    ~TmpState() { unref(Reg, _ref); }

private:
    int _ref;
};

class LuaCallback : public LuaState
{
  public:
    LuaCallback(lua_State *L);
    ~LuaCallback();

    int InitCallback(const char *nf = NULL);
    int InitCallback(int i);
    int BeginCallback(int nr, int msgh = 0);

  private:
    int m_ref;
    int m_top;
};

#endif /* __LUAAPI_H__ */
