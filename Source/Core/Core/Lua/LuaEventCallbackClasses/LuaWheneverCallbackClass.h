#pragma once
#include <mutex>
#include <string>

#include <lua.hpp>

namespace Lua::LuaWheneverCallback
{

void InitLuaWheneverCallbackFunctions(lua_State* lua_state, const std::string& lua_api_version,
                                      std::mutex* new_general_lua_lock);
int Register(lua_State* lua_state);
int Unregister(lua_State* lua_state);
int RunCallbacks();
}  // namespace Lua::LuaWheneverCallback
