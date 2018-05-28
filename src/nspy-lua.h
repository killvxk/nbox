#ifndef __NSPY_LUA_H__
#define __NSPY_LUA_H__

#include "luaapi.h"

class NBoxLua : public LuaState
{
public:
    NBoxLua();
    ~NBoxLua() { close(); }

    // invoke a global function
    int callglobal();
};

extern NBoxLua g_nblua;

#endif /* __NSPY_LUA_H__ */
