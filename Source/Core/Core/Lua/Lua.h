#pragma once

#include <lua.hpp>

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Core/Lua/LuaHelperClasses/LuaScriptState.h"
#include "Core/Lua/LuaHelperClasses/LuaStateToScriptContextMap.h"

namespace Lua
{
extern std::string global_lua_api_version;
extern int x;
extern bool is_lua_core_initialized;

extern std::vector<std::shared_ptr<LuaScriptState>> list_of_lua_script_contexts;

extern std::function<void(const std::string&)>* print_callback_function;
extern std::function<void(int)>* script_end_callback_function;

int SetLuaCoreVersion(lua_State* lua_state);
int GetLuaCoreVersion(lua_State* lua_state);

void Init(const std::string& script_location,
          std::function<void(const std::string&)>* new_print_callback,
          std::function<void(int)>* new_script_end_callback, int unique_script_identifier);

void StopScript(int unique_identifier);

}  // namespace Lua
