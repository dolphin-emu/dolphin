#pragma once
#include <string>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "common/CommonTypes.h"
#include "../CommonLuaHelpers.h"

namespace Lua
{

namespace LuaMemoryApi
{

class MEMORY
{
public:
  MEMORY() {}
};

inline MEMORY* instance = NULL;

void InitLuaMemoryApi(lua_State* luaState);

int do_general_read(lua_State* luaState);
int do_read_unsigned_bytes(lua_State* luaState);
int do_read_signed_bytes(lua_State* luaState);
int do_read_fixed_length_string(lua_State* luaState);
int do_read_null_terminated_string(lua_State* luaState);

int do_general_write(lua_State* luaState);
int do_write_bytes(lua_State* luaState);
int do_write_string(lua_State* luaState);
}  // namespace LuaMemoryApi
}  // namespace Lua
