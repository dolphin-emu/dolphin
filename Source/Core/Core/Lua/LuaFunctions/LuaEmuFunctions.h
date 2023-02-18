#pragma once

#include <lua.hpp>
#include <string>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaEmu
{
extern const char* class_name;

extern bool waiting_for_save_state_load;
extern bool waiting_for_save_state_save;
extern bool waiting_to_start_playing_movie;
extern bool waiting_to_save_movie;

void InitLuaEmuFunctions(lua_State* lua_state, const std::string& lua_api_version);
int EmuFrameAdvance(lua_State* lua_State);
int EmuLoadState(lua_State* lua_state);
int EmuSaveState(lua_State* lua_state);
int EmuPlayMovie(lua_State* lua_state);
int EmuSaveMovie(lua_State* lua_state);

}  // namespace Lua::LuaEmu
