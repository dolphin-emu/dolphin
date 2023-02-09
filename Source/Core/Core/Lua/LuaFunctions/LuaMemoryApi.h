#pragma once
#include <string>
#include <lua.hpp>
#include "Common/CommonTypes.h"
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaMemoryApi
{

class MEMORY
{
public:
  MEMORY() {}
};

void InitLuaMemoryApi(lua_State* luaState, const std::string& luaApiVersion);

int do_general_read(lua_State* luaState);
int do_read_unsigned_bytes(lua_State* luaState);
int do_read_signed_bytes(lua_State* luaState);
int do_read_fixed_length_string(lua_State* luaState);
int do_read_null_terminated_string(lua_State* luaState);

int do_general_write(lua_State* luaState);
int do_write_bytes(lua_State* luaState);
int do_write_string(lua_State* luaState);
}  // namespace LuaMemoryApi
