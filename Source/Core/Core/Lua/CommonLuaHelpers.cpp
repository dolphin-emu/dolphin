#include "CommonLuaHelpers.h"

namespace Lua
{
void luaColonOperatorTypeCheck(lua_State* luaState, const char* functionName, const char* exampleCall)
{
  if (lua_type(luaState, 1) != LUA_TUSERDATA)
  {
    luaL_error(
        luaState,
        (std::string("Error: User attempted to call ") + functionName +
         " function using the dot operator. Please use the colon operator instead like this: '" +
         exampleCall + "'")
            .c_str());
  }
}

}  // namespace Lua
