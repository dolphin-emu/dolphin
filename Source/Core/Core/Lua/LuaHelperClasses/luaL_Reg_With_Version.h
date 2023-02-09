#ifndef LUAL_REG_WITH_VERSION
#define LUAL_REG_WITH_VERSION

#include <lua.hpp>

typedef struct luaL_Reg_With_Version
{
  const char* name;
  const char* version;
  lua_CFunction func;
} luaL_Reg_With_Version;
#endif 
