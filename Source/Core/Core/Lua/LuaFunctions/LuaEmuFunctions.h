#pragma once
#include <mutex>
#include <condition_variable>

#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"

namespace Lua
{
  namespace LuaEmu
  {
  extern bool luaScriptActive;
  extern std::mutex frameAdvanceLock;
  extern std::condition_variable frameAdvanceConditionalVariable;
    class emu
    {
    public:
      inline emu() {}
    };

    extern emu* emuPointer;

    emu* GetEmuInstance();
    void InitLuaEmuFunctions(lua_State* luaState);
    int emu_frameAdvance(lua_State* luaState);
    int emu_get_register(lua_State* luaState);
  }
}
