#pragma once

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}
#include "../CommonLuaHelpers.h"

namespace Lua
{
  namespace LuaEmu
  {
  extern bool waitingForSaveStateLoad;
  extern bool waitingForSaveStateSave;
  extern bool waitingToStartPlayingMovie;
  extern bool waitingToSaveMovie;

    void InitLuaEmuFunctions(lua_State* luaState);
    int emu_frameAdvance(lua_State* luaState);
    int emu_loadState(lua_State* luaState);
    int emu_saveState(lua_State* luaState);
    int emu_playMovie(lua_State* luaState);
    int emu_saveMovie(lua_State* luaState);
  }
}
