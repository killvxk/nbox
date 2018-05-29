#ifndef ___LUAUSER_H__
#define ___LUAUSER_H__

#define lua_lock(L) LuaLock(L)
#define lua_unlock(L) LuaUnlock(L)
#define luai_userstateopen(L) LuaLockInitial(L)
#define luai_userstatethread(L,L1) LuaLockInitial(L1)  // Lua 5.1

void LuaLockInitial(lua_State * L);
void LuaLockFinal(lua_State * L);
void LuaLock(lua_State * L);
void LuaUnlock(lua_State * L);

#endif /* ___LUAUSER_H__ */
