#pragma once
#include <mutex>
#include <string>

#include <lua.hpp>

namespace Lua::LuaOnFrameStartCallback
{

extern lua_State* on_frame_start_thread;
extern int on_frame_start_lua_function_reference;
extern bool frame_start_callback_is_registered;

void InitLuaOnFrameStartCallbackFunctions(lua_State* lua_state, const std::string& lua_api_version,
                                          std::mutex* new_lua_general_lock);
int Register(lua_State* lua_state);
int Unregister(lua_State* lua_state);
int RunCallback();
}  // namespace Lua::LuaOnFrameStartCallback
