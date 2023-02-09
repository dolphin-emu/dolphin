#pragma once

#include <lua.hpp>
#include <string>

namespace Lua
{
void luaColonOperatorTypeCheck(lua_State* luaState, const char* functionName, const char* exampleCall);
}
