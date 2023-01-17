#pragma once
extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}
#include <string>

namespace Lua
{
void luaColonOperatorTypeCheck(lua_State* luaState, const char* functionName, const char* exampleCall);
}
