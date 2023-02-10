#pragma once

#include <lua.hpp>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace Lua
{
extern std::string global_lua_api_version;
extern lua_State* main_lua_state;
extern lua_State* main_lua_thread_state;
extern int x;
extern bool is_lua_script_active;
extern bool is_lua_core_initialized;
extern std::function<void(const std::string&)>* print_callback_function;
extern std::function<void()>* script_end_callback_function;

int SetLuaCoreVersion(lua_State* lua_state);
int GetLuaCoreVersion(lua_State* lua_state);

void Init(const std::string& script_location,
          std::function<void(const std::string&)>* new_print_callback,
          std::function<void()>* new_script_end_callback);

void StopScript();

} // namespace Lua
