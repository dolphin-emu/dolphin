#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

#include <fmt/format.h>

namespace Lua
{
void LuaColonOperatorTypeCheck(lua_State* lua_state, const char* class_name,
                               const char* func_name, const char* example_args)
{
  if (lua_type(lua_state, 1) != LUA_TUSERDATA)
  {
    luaL_error(lua_state,
               fmt::format("Error: User attempted to call {}:{} function using the dot operator. "
                           "Please use the colon operator instead like this: '{}:{}{}'",
                           class_name, func_name, class_name, func_name, example_args
                           )
                   .c_str());
  }
}

}  // namespace Lua
