#pragma once
#include <lua.hpp>

#include <string>

#include "Common/CommonTypes.h"
#include "Core/Lua/LuaHelperClasses/LuaScriptCallLocations.h"

namespace Lua::LuaMemoryApi
{
extern const char* class_name;

void InitLuaMemoryApi(lua_State* lua_state, const std::string& lua_api_version,
                      LuaScriptCallLocations* new_script_call_location_pointer);

int DoGeneralRead(lua_State* lua_state);
int DoReadUnsignedBytes(lua_State* lua_state);
int DoReadSignedBytes(lua_State* lua_state);
int DoReadFixedLengthString(lua_State* lua_state);
int DoReadNullTerminatedString(lua_State* lua_state);

int DoGeneralWrite(lua_State* lua_state);
int DoWriteBytes(lua_State* lua_state);
int DoWriteString(lua_State* lua_state);
}  // namespace Lua::LuaMemoryApi
