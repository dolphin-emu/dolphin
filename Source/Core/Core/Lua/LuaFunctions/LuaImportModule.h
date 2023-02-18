#pragma once
#include <string>

#include <lua.hpp>

#include "Core/Lua/LuaHelperClasses/LuaScriptCallLocations.h"

namespace Lua::LuaImportModule
{
extern const char* class_name;

void InitLuaImportModule(lua_State* lua_state, const std::string& lua_api_version);
int ImportModule(lua_State* lua_state);
int ImportAlt(lua_State* luaState);

}  // namespace Lua::LuaImportModule
