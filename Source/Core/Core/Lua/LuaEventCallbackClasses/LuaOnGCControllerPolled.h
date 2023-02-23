#pragma once
#include <functional>
#include <lua.hpp>
#include <mutex>
#include <string>
#include <vector>

#include "Core/Lua/LuaHelperClasses/LuaScriptContext.h"

namespace Lua::LuaOnGCControllerPolled
{
extern std::mutex gc_controller_polled_lock;

void InitLuaOnGCControllerPolledCallbackFunctions(
    lua_State* main_lua_thread, const std::string& lua_api_version,
    std::vector<std::shared_ptr<LuaScriptContext>>* new_pointer_to_lua_script_list,
    std::function<void(const std::string&)>* new_print_callback_function,
    std::function<void(int)>* new_script_end_callback_function);
int Register(lua_State* lua_state);
int Unregister(lua_State* lua_state);
int RunCallbacks();
}  // namespace Lua::LuaOnGCControllerPolled
