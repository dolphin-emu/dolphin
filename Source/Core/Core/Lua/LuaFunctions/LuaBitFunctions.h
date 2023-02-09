#pragma once
#include <string>
#include <lua.hpp>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaBit
{

void InitLuaBitFunctions(lua_State* luaState, const std::string& luaApiVersion);
int bitwise_and(lua_State* luaState);
int bitwise_or(lua_State* luaState);
int bitwise_not(lua_State* luaState);
int bitwise_xor(lua_State* luaState);
int logical_and(lua_State* luaState);
int logical_or(lua_State* luaState);
int logical_xor(lua_State* luaState);
int logical_not(lua_State* luaState);
int bit_shift_left(lua_State* luaState);
int bit_shift_right(lua_State* luaState);

}
