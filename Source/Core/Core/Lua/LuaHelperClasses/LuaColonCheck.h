#pragma once

#include <lua.hpp>
#include <string>

namespace Lua
{
void LuaColonOperatorTypeCheck(lua_State* lua_state, const char* class_name, const char* func_name,
                               const char* example_args);
}
