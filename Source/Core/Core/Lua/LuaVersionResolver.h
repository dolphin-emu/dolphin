#pragma once

#include <vector>
#include <string>
#include <unordered_map>

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

namespace Lua
{

typedef struct luaL_Reg_With_Version
{
  const char* name;
  const char* version;
  lua_CFunction func;
} luaL_Reg_With_Version;

std::vector<luaL_Reg> getLatestFunctionsForVersion(const luaL_Reg_With_Version allFunctions[], size_t arraySize,
                             const std::string& LUA_API_VERSION,
                              std::unordered_map<std::string, std::string>&
                                 deprecatedFunctionsToVersionTheyWereRemovedInMap);
}
