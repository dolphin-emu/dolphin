#pragma once

#include <vector>
#include <string>

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

namespace Lua
{
  std::vector<luaL_Reg> getLatestFunctionsForVersion(const luaL_Reg allFunctions[], size_t arraySize, const std::string& LUA_API_VERSION);
}
