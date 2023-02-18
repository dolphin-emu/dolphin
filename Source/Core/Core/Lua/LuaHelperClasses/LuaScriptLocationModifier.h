#ifndef LUA_SCRIPT_LOCATION_MODIFIER
#define LUA_SCRIPT_LOCATION_MODIFIER
#include <lua.hpp>

#include "Common/CommonTypes.h"
#include "Core/Lua/LuaHelperClasses/LuaScriptCallLocations.h"
namespace Lua
{

static const char* script_location_var = "ScriptCallLocation";
LuaScriptCallLocations GetScriptCallLocation(lua_State* lua_state);
void SetScriptCallLocation(lua_State* lua_state, LuaScriptCallLocations new_script_call_location);

}
#endif
