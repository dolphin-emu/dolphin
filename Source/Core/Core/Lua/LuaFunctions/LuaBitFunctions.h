#pragma once
#include <lua.hpp>
#include <string>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaHelperClasses/LuaScriptCallLocations.h"

namespace Lua::LuaBit
{
extern const char* class_name;

void InitLuaBitFunctions(lua_State* lua_state, const std::string& lua_api_version,
                         LuaScriptCallLocations* new_script_call_location_pointer);
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

}  // namespace Lua::LuaBit
