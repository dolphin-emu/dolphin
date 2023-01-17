#pragma once
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "../LuaHelperClasses/LuaColonCheck.h"

namespace Lua
{
namespace LuaBit
{

class BitClass
{
public:
  BitClass() {}
};

static BitClass* bitInstance;

BitClass* GetInstance();

void InitLuaBitFunctions(lua_State* luaState);
int bitwise_and(lua_State* luaState);
int bitwise_or(lua_State* luaState);
int bitwise_not(lua_State* luaState);
int bitwise_xor(lua_State* luaState);
int logical_and(lua_State* luaState);
int logical_or(lua_State* luaState);
int logical_not(lua_State* luaState);
int bit_shift_left(lua_State* luaState);
int bit_shift_right(lua_State* luaState);
}
}
