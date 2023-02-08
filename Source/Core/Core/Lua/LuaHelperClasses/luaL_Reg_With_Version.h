#ifndef LUAL_REG_WITH_VERSION
#define LUAL_REG_WITH_VERSION



extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}


typedef struct luaL_Reg_With_Version
{
  const char* name;
  const char* version;
  lua_CFunction func;
} luaL_Reg_With_Version;
#endif 
