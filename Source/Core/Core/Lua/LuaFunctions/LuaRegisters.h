#pragma once
#include <string>

#include <lua.hpp>

#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaRegisters
{
void InitLuaRegistersFunctions(lua_State* luaState, const std::string& luaApiVersion);
int getRegister(lua_State* luaState);
int getRegisterAsUnsignedByteArray(lua_State* luaState);
int getRegisterAsSignedByteArray(lua_State* luaState);
int setRegister(lua_State* luaState);
int setRegisterFromByteArray(lua_State* luaState);
}
