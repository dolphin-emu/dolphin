#pragma once
#include "ByteWrapper.h"

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

namespace Lua
{

namespace LuaByteWrapper
{
static const char* LUA_BYTE_WRAPPER = "ByteWrapper";
void InitLuaByteWrapperClass(lua_State* luaState);

int newByteWrapper(lua_State* luaState);
int deleteByteWrapper(lua_State* luaState);
int withType(lua_State* luaState);

int getValue(lua_State* luaState);

int operatorOverloadedBitwiseAnd(lua_State* luaState);
int operatorOverloadedBitwiseOr(lua_State* luaState);
int operatorOverloadedBitwiseXor(lua_State* luaState);
int operatorOverloadedBitwiseNot(lua_State* luaState);
int operatorOverloadedLogicalAnd(lua_State* luaState);
int operatorOverloadedLogicalOr(lua_State* luaState);
int operatorOverloadedLogicalNot(lua_State* luaState);
int operatorOverloadedBitShiftLeft(lua_State* luaState);
int operatorOverloadedBitShiftRight(lua_State* luaState);
int operatorOverloadedEqualsEquals(lua_State* luaState);
int operatorOverloadedLessThan(lua_State* luaState);
int operatorOverloadedLessThanEquals(lua_State* luaState);
}  // namespace LuaByteWrapper
}  // namespace Lua
