#include "Core/Lua/LuaHelperClasses/LuaScriptLocationModifier.h"

namespace Lua
{

LuaScriptCallLocations GetScriptCallLocation(lua_State* lua_state)
{
  lua_getglobal(lua_state, script_location_var);
  s64 raw_script_call_location_value = lua_tointeger(lua_state, 1);
  lua_pop(lua_state, 1);
  return static_cast<LuaScriptCallLocations>(raw_script_call_location_value);
}

void SetScriptCallLocation(lua_State* lua_state, LuaScriptCallLocations new_script_call_location)
{
  lua_pushinteger(lua_state, static_cast<lua_Integer>(new_script_call_location));
  lua_setglobal(lua_state, script_location_var);
  return;
}
}
