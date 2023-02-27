#pragma once
#include <lua.hpp>
#include <memory>
#include <unordered_map>
#include "Core/Lua/LuaHelperClasses/LuaScriptState.h"

namespace Lua
{
class LuaStateToScriptContextMap
{
public:
  LuaStateToScriptContextMap() {
    lua_state_to_script_context_pointer_map =
        std::unordered_map<lua_State*, LuaScriptState*>();
  }

  std::unordered_map<lua_State*, LuaScriptState*>
      lua_state_to_script_context_pointer_map;
};
}
