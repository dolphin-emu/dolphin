#pragma once
#include <lua.hpp>
#include <string>

#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Movie.h"

namespace Lua::LuaStatistics
{

void InitLuaStatisticsFunctions(lua_State* lua_state, const std::string& lua_api_version);

int IsRecordingInput(lua_State* lua_state);
int IsRecordingInputFromSaveState(lua_State* lua_state);
int IsPlayingInput(lua_State* lua_state);
int IsMovieActive(lua_State* lua_state);

int GetCurrentFrame(lua_State* lua_state);
int GetMovieLength(lua_State* lua_state);
int GetRerecordCount(lua_State* lua_state);
int GetCurrentInputCount(lua_State* lua_state);
int GetTotalInputCount(lua_State* lua_state);
int GetCurrentLagCount(lua_State* lua_state);
int GetTotalLagCount(lua_State* lua_state);

int IsGcControllerInPort(lua_State* lua_state);
int IsUsingPort(lua_State* lua_state);
}  // namespace Lua::LuaStatistics
