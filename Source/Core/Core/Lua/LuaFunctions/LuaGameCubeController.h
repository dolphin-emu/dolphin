#pragma once

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

namespace Lua
{
namespace LuaGameCubeController
{

  class gc_controller_lua
  {
  public:
    inline gc_controller_lua(){};
  };

  extern gc_controller_lua* gc_controller_pointer;

  gc_controller_lua* getControllerInstance();
  void InitLuaGameCubeControllerFunctions(lua_State* luaState);
  int setInputs(lua_State* luaState);
  }
}
