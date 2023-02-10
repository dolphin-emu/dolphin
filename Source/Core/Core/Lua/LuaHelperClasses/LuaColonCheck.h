#pragma once

#include <lua.hpp>
#include <string>

namespace Lua
{
void LuaColonOperatorTypeCheck(lua_State* lua_state, const char* function_name,
                               const char* example_call);
}
