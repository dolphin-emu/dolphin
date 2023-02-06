#pragma once
#include <string>
extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

#include "../LuaHelperClasses/LuaColonCheck.h"
#include "Core/Movie.h"

namespace Lua
{
namespace LuaStatistics
{

void InitLuaStatisticsFunctions(lua_State* luaState, const std::string& luaApiVersion);

int isRecordingInput(lua_State* luaState);
int isRecordingInputFromSaveState(lua_State* luaState);
int isPlayingInput(lua_State* luaState);
int isMovieActive(lua_State* luaState);

int getCurrentFrame(lua_State* luaState);
int getMovieLength(lua_State* luaState);
int getRerecordCount(lua_State* luaState);
int getCurrentInputCount(lua_State* luaState);
int getTotalInputCount(lua_State* luaState);
int getCurrentLagCount(lua_State* luaState);
int getTotalLagCount(lua_State* luaState);

int isGcControllerInPort(lua_State* luaState);
int isUsingPort(lua_State* luaState);
}
}
