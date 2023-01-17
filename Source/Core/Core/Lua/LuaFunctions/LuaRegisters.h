#pragma once
extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

namespace Lua
{
namespace LuaRegisters
{
void InitLuaRegistersFunctions(lua_State* luaState);
int getRegister(lua_State* luaState);
int getRegisterAsUnsignedByteArray(lua_State* luaState);
int getRegisterAsSignedByteArray(lua_State* luaState);
int setRegister(lua_State* luaState);
int setRegisterFromByteArray(lua_State* luaState);
}
}
