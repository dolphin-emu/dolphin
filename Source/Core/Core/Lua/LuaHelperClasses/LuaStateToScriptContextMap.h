#pragma once
#include <lua.hpp>
#include <memory>
#include <unordered_map>
#include "Core/Lua/LuaHelperClasses/LuaScriptContext.h"

namespace Lua
{
class LuaStateToScriptContextMap
{
public:
  LuaStateToScriptContextMap() {
    lua_state_to_script_context_pointer_map =
        std::unordered_map<lua_State*, LuaScriptContext*>();
  }

  std::unordered_map<lua_State*, LuaScriptContext*>
      lua_state_to_script_context_pointer_map;
};
}
