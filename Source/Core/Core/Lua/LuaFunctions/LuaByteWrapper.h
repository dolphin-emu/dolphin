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

int withTypeU8(lua_State* luaState);
int withTypeUnsigned8(lua_State* luaState);
int withTypeUnsignedByte(lua_State* luaState);

int withTypeS8(lua_State* luaState);
int withTypeSigned8(lua_State* luaState);
int withTypeSignedByte(lua_State* luaState);

int withTypeU16(lua_State* luaState);
int withTypeUnsigned16(lua_State* luaState);

int withTypeS16(lua_State* luaState);
int withTypeSigned16(lua_State* luaState);

int withTypeU32(lua_State* luaState);
int withTypeUnsigned32(lua_State* luaState);
int withTypeUnsignedInt(lua_State* luaState);

int withTypeS32(lua_State* luaState);
int withTypeSigned32(lua_State* luaState);
int withTypeSignedInt(lua_State* luaState);

int withTypeFloat(lua_State* luaState);

int withTypeU64(lua_State* luaState);
int withTypeUnsigned64(lua_State* luaState);
int withTypeUnsignedLongLong(lua_State* luaState);

int withTypeS64(lua_State* luaState);
int withTypeSigned64(lua_State* luaState);
int withTypeSignedLongLong(lua_State* luaState);

int withTypeDouble(lua_State* luaState);

int getValue(lua_State* luaState);

int asU8(lua_State* luaState);
int asUnsigned8(lua_State* luaState);
int asUnsignedByte(lua_State* luaState);

int asS8(lua_State* luaState);
int asSigned8(lua_State* luaState);
int asSignedByte(lua_State* luaState);

int asU16(lua_State* luaState);
int asUnsigned16(lua_State* luaState);

int asS16(lua_State* luaState);
int asSigned16(lua_State* luaState);

int asU32(lua_State* luaState);
int asUnsigned32(lua_State* luaState);
int asUnsignedInt(lua_State* luaState);

int asS32(lua_State* luaState);
int asSigned32(lua_State* luaState);
int asSignedInt(lua_State* luaState);

int asFloat(lua_State* luaState);

int asU64(lua_State* luaState);
int asUnsigned64(lua_State* luaState);
int asUnsignedLongLong(lua_State* luaState);

int asS64(lua_State* luaState);
int asSigned64(lua_State* luaState);
int asSignedLongLong(lua_State* luaState);

int asDouble(lua_State* luaState);

int operatorOverloadedBitwiseAnd(lua_State* luaState);
/* int operatorOverloadedBitwiseOr(lua_State* luaState);
int operatorOverloadedBitwiseXor(lua_State* luaState);
int operatorOverloadedBitwiseNot(lua_State* luaState);
int operatorOverloadedLogicalAnd(lua_State* luaState);
int operatorOverloadedLogicalOr(lua_State* luaState);
int operatorOverloadedLogicalNot(lua_State* luaState);
int operatorOverloadedBitShiftLeft(lua_State* luaState);
int operatorOverloadedBitShiftRight(lua_State* luaState);
int operatorOverloadedEqualsEquals(lua_State* luaState);
int operatorOverloadedLessThan(lua_State* luaState);
int operatorOverloadedLessThanEquals(lua_State* luaState);*/
}  // namespace LuaByteWrapper
}  // namespace Lua
