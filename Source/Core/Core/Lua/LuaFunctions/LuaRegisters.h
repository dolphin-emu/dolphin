#pragma once
#include <string>

#include <lua.hpp>

#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"

namespace Lua::LuaRegisters
{

extern const char* class_name;
void InitLuaRegistersFunctions(lua_State* lua_state, const std::string& lua_api_version);
int GetRegister(lua_State* lua_state);
int GetRegisterAsUnsignedByteArray(lua_State* lua_state);
int GetRegisterAsSignedByteArray(lua_State* lua_state);
int SetRegister(lua_State* lua_state);
int SetRegisterFromByteArray(lua_State* lua_state);
}  // namespace Lua::LuaRegisters
