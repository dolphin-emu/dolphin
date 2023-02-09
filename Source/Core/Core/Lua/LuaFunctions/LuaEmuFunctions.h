#pragma once

#include <string>
#include <lua.hpp>

#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaEmu
  {
  extern bool waitingForSaveStateLoad;
  extern bool waitingForSaveStateSave;
  extern bool waitingToStartPlayingMovie;
  extern bool waitingToSaveMovie;

    void InitLuaEmuFunctions(lua_State* luaState, const std::string& luaApiVersion);
    int emu_frameAdvance(lua_State* luaState);
    int emu_loadState(lua_State* luaState);
    int emu_saveState(lua_State* luaState);
    int emu_playMovie(lua_State* luaState);
    int emu_saveMovie(lua_State* luaState);
  }
