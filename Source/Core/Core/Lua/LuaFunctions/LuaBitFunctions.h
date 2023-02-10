#pragma once
#include <lua.hpp>
#include <string>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaBit
{

void InitLuaBitFunctions(lua_State* lua_state, const std::string& lua_api_version);
int BitwiseAnd(lua_State* lua_state);
int BitwiseOr(lua_State* lua_state);
int BitwiseNot(lua_State* lua_state);
int BitwiseXor(lua_State* lua_state);
int LogicalAnd(lua_State* lua_state);
int LogicalOr(lua_State* lua_state);
int LogicalXor(lua_State* lua_state);
int LogicalNot(lua_State* lua_state);
int BitShiftLeft(lua_State* lua_state);
int BitShiftRight(lua_State* lua_state);

} // namespace Lua::LuaBit
