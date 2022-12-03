#include "LuaByteWrapper.h"
#include <format>
namespace Lua
{
namespace LuaByteWrapper
{
void InitLuaByteWrapperClass(lua_State* luaState)
{
  lua_register(luaState, LUA_BYTE_WRAPPER, newByteWrapper);
  luaL_newmetatable(luaState, LUA_BYTE_WRAPPER);
  lua_pushcfunction(luaState, deleteByteWrapper);
  lua_setfield(luaState, -2, "__gc");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  lua_pushcfunction(luaState, withTypeU8);
  lua_setfield(luaState, -2, "with_type_u8");

  lua_pushcfunction(luaState, withType);
  lua_setfield(luaState, -2, "with_type");

  lua_pushcfunction(luaState, withTypeUnsigned8);
  lua_setfield(luaState, -2, "with_type_unsigned_8");
  lua_pushcfunction(luaState, withTypeUnsignedByte);
  lua_setfield(luaState, -2, "with_type_unsigned_byte");
  lua_pushcfunction(luaState, withTypeS8);
  lua_setfield(luaState, -2, "with_type_s8");
  lua_pushcfunction(luaState, withTypeSigned8);
  lua_setfield(luaState, -2, "with_type_signed_8");
  lua_pushcfunction(luaState, withTypeSignedByte);
  lua_setfield(luaState, -2, "with_type_signed_byte");
  lua_pushcfunction(luaState, withTypeU16);
  lua_setfield(luaState, -2, "with_type_u16");
  lua_pushcfunction(luaState, withTypeUnsigned16);
  lua_setfield(luaState, -2, "with_type_unsigned_16");
  lua_pushcfunction(luaState, withTypeS16);
  lua_setfield(luaState, -2, "with_type_s16");
  lua_pushcfunction(luaState, withTypeSigned16);
  lua_setfield(luaState, -2, "with_type_signed_16");
  lua_pushcfunction(luaState, withTypeU32);
  lua_setfield(luaState, -2, "with_type_u32");
  lua_pushcfunction(luaState, withTypeUnsigned32);
  lua_setfield(luaState, -2, "with_type_unsigned_32");
  lua_pushcfunction(luaState, withTypeUnsignedInt);
  lua_setfield(luaState, -2, "with_type_unsigned_int");
  lua_pushcfunction(luaState, withTypeS32);
  lua_setfield(luaState, -2, "with_type_s32");
  lua_pushcfunction(luaState, withTypeSigned32);
  lua_setfield(luaState, -2, "with_type_signed_32");
  lua_pushcfunction(luaState, withTypeSignedInt);
  lua_setfield(luaState, -2, "with_type_signed_int");
  lua_pushcfunction(luaState, withTypeFloat);
  lua_setfield(luaState, -2, "with_type_float");
  lua_pushcfunction(luaState, withTypeU64);
  lua_setfield(luaState, -2, "with_type_u64");
  lua_pushcfunction(luaState, withTypeUnsigned64);
  lua_setfield(luaState, -2, "with_type_unsigned_64");
  lua_pushcfunction(luaState, withTypeUnsignedLongLong);
  lua_setfield(luaState, -2, "with_type_unsigned_long_long");
  lua_pushcfunction(luaState, withTypeS64);
  lua_setfield(luaState, -2, "with_type_s64");
  lua_pushcfunction(luaState, withTypeSigned64);
  lua_setfield(luaState, -2, "with_type_signed_64");
  lua_pushcfunction(luaState, withTypeSignedLongLong);
  lua_setfield(luaState, -2, "with_type_signed_long_long");
  lua_pushcfunction(luaState, withTypeDouble);
  lua_setfield(luaState, -2, "with_type_double");

  lua_pushcfunction(luaState, getValue);
  lua_setfield(luaState, -2, "get_value");

  lua_pushcfunction(luaState, asU8);
  lua_setfield(luaState, -2, "get_value_as_u8");

  lua_pushcfunction(luaState, asUnsigned8);
  lua_setfield(luaState, -2, "get_value_as_unsigned_8");

  lua_pushcfunction(luaState, asUnsignedByte);
  lua_setfield(luaState, -2, "get_value_as_unsigned_byte");

  lua_pushcfunction(luaState, asS8);
  lua_setfield(luaState, -2, "get_value_as_s8");

  lua_pushcfunction(luaState, asSigned8);
  lua_setfield(luaState, -2, "get_value_as_signed_8");

  lua_pushcfunction(luaState, asSignedByte);
  lua_setfield(luaState, -2, "get_value_as_signed_byte");

  lua_pushcfunction(luaState, asU16);
  lua_setfield(luaState, -2, "get_value_as_u16");

  lua_pushcfunction(luaState, asUnsigned16);
  lua_setfield(luaState, -2, "get_value_as_unsigned_16");

  lua_pushcfunction(luaState, asS16);
  lua_setfield(luaState, -2, "get_value_as_s16");

  lua_pushcfunction(luaState, asSigned16);
  lua_setfield(luaState, -2, "get_value_as_signed_16");

  lua_pushcfunction(luaState, asU32);
  lua_setfield(luaState, -2, "get_value_as_u32");

  lua_pushcfunction(luaState, asUnsigned32);
  lua_setfield(luaState, -2, "get_value_as_unsigned_32");

  lua_pushcfunction(luaState, asUnsignedInt);
  lua_setfield(luaState, -2, "get_value_as_unsigned_int");

  lua_pushcfunction(luaState, asS32);
  lua_setfield(luaState, -2, "get_value_as_s32");

  lua_pushcfunction(luaState, asSigned32);
  lua_setfield(luaState, -2, "get_value_as_signed_32");

  lua_pushcfunction(luaState, asSignedInt);
  lua_setfield(luaState, -2, "get_value_as_signed_int");

  lua_pushcfunction(luaState, asFloat);
  lua_setfield(luaState, -2, "get_value_as_float");

  lua_pushcfunction(luaState, asU64);
  lua_setfield(luaState, -2, "get_value_as_u64");

  lua_pushcfunction(luaState, asUnsigned64);
  lua_setfield(luaState, -2, "get_value_as_unsigned_64");

  lua_pushcfunction(luaState, asUnsignedLongLong);
  lua_setfield(luaState, -2, "get_value_as_unsigned_long_long");

  lua_pushcfunction(luaState, asS64);
  lua_setfield(luaState, -2, "get_value_as_s64");

  lua_pushcfunction(luaState, asSigned64);
  lua_setfield(luaState, -2, "get_value_as_signed_64");

  lua_pushcfunction(luaState, asSignedLongLong);
  lua_setfield(luaState, -2, "get_value_as_signed_long_long");

  lua_pushcfunction(luaState, asDouble);
  lua_setfield(luaState, -2, "get_value_as_double");




  lua_pushcfunction(luaState, operatorOverloadedBitwiseAnd);
  lua_setfield(luaState, -2, "__band");
  lua_pop(luaState, 1);
}

int newByteWrapper(lua_State* luaState)
{
  if (lua_isinteger(luaState, 1))
  {
    long long longLongInitialValue = luaL_checkinteger(luaState, 1);
    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = ByteWrapper::CreateByteWrapperFromLongLong(longLongInitialValue);
  }
  else if (lua_isnumber(luaState, 1))
  {
    double doubleInitialValue = luaL_checknumber(luaState, 1);
    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = ByteWrapper::CreateBytewrapperFromDouble(doubleInitialValue);
  }
  else if (lua_isuserdata(luaState, 1))
  {
    ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = ByteWrapper::CreateByteWrapperFromCopy(byteWrapperPointer);
  }
  else
  {
    luaL_error(luaState, "Error: Constructor for ByteWrapper() was passed an invalid argument. Valid arguments are integer, number or another ByteWrapper.");
    return 1;
  }

  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int deleteByteWrapper(lua_State* luaState)
{
  delete *reinterpret_cast<ByteWrapper**>(lua_touserdata(luaState, 1));
  return 0;
}

int withType(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  const char* typeString = luaL_checkstring(luaState, 2);
  ByteWrapper::ByteType parsedType = ByteWrapper::parseType(typeString);
  if (parsedType == ByteWrapper::ByteType::UNDEFINED)
  {
    luaL_error(luaState, "Error: Argument to withType() must be a string which is either u8, s8, u16, s16, u32, s32, float, u64, s64, or double.");
    return 0;
  }
  else if (!ByteWrapper::typeSizeCheck(luaState, byteWrapperPointer, parsedType, "Error: Cannot call withType() function with argument {} on ByteWrapper which holds less than {} bytes"))
  {
    return 0;
  }

  byteWrapperPointer->setType(parsedType);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeU8Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 1)
  {
    luaL_error(luaState, "Error: Cannot set type to unsigned_8 on ByteWrapper with size less than 1");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::UNSIGNED_8);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeU8(lua_State* luaState)
{
  return withTypeU8Helper(luaState);
}

int withTypeUnsigned8(lua_State* luaState)
{
  return withTypeU8Helper(luaState);
}
int withTypeUnsignedByte(lua_State* luaState)
{
  return withTypeU8Helper(luaState);
}

int withTypeSigned8Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 1)
  {
    luaL_error(luaState,"Error: Cannot set type to signed_8 on ByteWrapper with size less than 1");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::SIGNED_8);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeS8(lua_State* luaState)
{
  return withTypeSigned8Helper(luaState);
}

int withTypeSigned8(lua_State* luaState)
{
  return withTypeSigned8Helper(luaState);
}

int withTypeSignedByte(lua_State* luaState)
{
  return withTypeSigned8Helper(luaState);
}

int withTypeUnsigned16Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 2)
  {
    luaL_error(luaState, "Error: Cannot set type to unsigned_16 on ByteWrapper with size less than 2");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::UNSIGNED_16);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeU16(lua_State* luaState)
{
  return withTypeUnsigned16Helper(luaState);
}

int withTypeUnsigned16(lua_State* luaState)
{
  return withTypeUnsigned16Helper(luaState);
}

int withTypeSigned16Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 2)
  {
    luaL_error(luaState, "Error: Cannot set type to signed_16 on ByteWrapper with size less than 2");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::SIGNED_16);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeS16(lua_State* luaState)
{
  return withTypeSigned16Helper(luaState);
}

int withTypeSigned16(lua_State* luaState)
{
  return withTypeSigned16Helper(luaState);
}


int withTypeUnsigned32Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 4)
  {
    luaL_error(luaState, "Error: Cannot set type to unsigned_32 on ByteWrapper with size less than 4");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::UNSIGNED_32);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeU32(lua_State* luaState)
{
  return withTypeUnsigned32Helper(luaState);
}

int withTypeUnsigned32(lua_State* luaState)
{
  return withTypeUnsigned32Helper(luaState);
}

int withTypeUnsignedInt(lua_State* luaState)
{
  return withTypeUnsigned32Helper(luaState);
}

int withTypeSigned32Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 4)
  {
    luaL_error(luaState,
               "Error: Cannot set type to signed_32 on ByteWrapper with size less than 4");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::SIGNED_32);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeS32(lua_State* luaState)
{
  return withTypeSigned32Helper(luaState);
}

int withTypeSigned32(lua_State* luaState)
{
  return withTypeSigned32Helper(luaState);
}

int withTypeSignedInt(lua_State* luaState)
{
  return withTypeSigned32Helper(luaState);
}

int withTypeFloat(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 4)
  {
    luaL_error(luaState, "Error: Cannot set type to float on ByteWrapper with size less than 4");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::FLOAT);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeUnsigned64Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 8)
  {
    luaL_error(luaState,
               "Error: Cannot set type to unsigned_64 on ByteWrapper with size less than 8");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::UNSIGNED_64);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeU64(lua_State* luaState)
{
  return withTypeUnsigned64Helper(luaState);
}

int withTypeUnsigned64(lua_State* luaState)
{
  return withTypeUnsigned64Helper(luaState);
}

int withTypeUnsignedLongLong(lua_State* luaState)
{
  return withTypeUnsigned64Helper(luaState);
}

int withTypeSigned64Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 8)
  {
    luaL_error(luaState, "Error: Cannot set type to signed_64 on ByteWrapper with size less than 8");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::SIGNED_64);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int withTypeS64(lua_State* luaState)
{
  return withTypeSigned64Helper(luaState);
}

int withTypeSigned64(lua_State* luaState)
{
  return withTypeSigned64Helper(luaState);
}

int withTypeSignedLongLong(lua_State* luaState)
{
  return withTypeSigned64Helper(luaState);
}

int withTypeDouble(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 8)
  {
    luaL_error(luaState, "Error: Cannot set type to double on ByteWrapper with size less than 8");
    return 0;
  }
  byteWrapperPointer->setType(ByteWrapper::ByteType::DOUBLE);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int getValue(lua_State* luaState)
{
  bool noStringArgument = false;
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  ByteWrapper::ByteType typeArgument = ByteWrapper::ByteType::UNDEFINED;
  u8 u8Val = 0;
  u16 u16Val = 0;
  u32 u32Val = 0;
  u64 u64Val = 0;
  s8 s8Val = 0;
  s16 s16Val = 0;
  s32 s32Val = 0;
  s64 s64Val = 0;
  float floatVal = 0.0;
  double doubleVal = 0.0;

  if (lua_gettop(luaState) < 2) //case where no argument for type was passed - meaning we should return the value in the type of the ByteWrapper
  {
    noStringArgument = true;
    typeArgument = byteWrapperPointer->byteType;
  }
  else
  {
    const char* typeString = luaL_checkstring(luaState, 2);
    if (typeString == NULL)
    {
      luaL_error(luaState, "Error: argument to get_value() must be a string which is equal to u8, u16, u32, u64, s8, s16, s32, s64, float or double.");
      return 0;
    }
    typeArgument = ByteWrapper::parseType(typeString);
  }

  if (typeArgument == ByteWrapper::ByteType::UNDEFINED)
  {
    if (noStringArgument)
    {
      luaL_error(luaState, "Error: get_value() was called on a ByteWrapper without first setting the type of the ByteWrapper using the with_type() function.");
    }
    else
    {
      luaL_error(luaState, "Error: argument to get_value() must be a string which is equal to u8, u16, u32, u64, s8, s16, s32, s64, float or double.");
    }
    return 0;
  }

  switch (typeArgument)
  {
  case ByteWrapper::ByteType::UNSIGNED_8:
    u8Val = byteWrapperPointer->getValueAsU8();
    lua_pushinteger(luaState, static_cast<lua_Integer>(u8Val));
    return 1;
  case ByteWrapper::ByteType::SIGNED_8:
    s8Val = byteWrapperPointer->getValueAsS8();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s8Val));
    return 1;
  case ByteWrapper::ByteType::UNSIGNED_16:
    u16Val = byteWrapperPointer->getValueAsU16();
    lua_pushinteger(luaState, static_cast<lua_Integer>(u16Val));
    return 1;
  case ByteWrapper::ByteType::SIGNED_16:
    s16Val = byteWrapperPointer->getValueAsS16();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s16Val));
    return 1;
  case ByteWrapper::ByteType::UNSIGNED_32:
    u32Val = byteWrapperPointer->getValueAsU32();
    lua_pushinteger(luaState, static_cast<lua_Integer>(u32Val));
    return 1;
  case ByteWrapper::ByteType::SIGNED_32:
    s32Val = byteWrapperPointer->getValueAsS32();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s32Val));
    return 1;
  case ByteWrapper::ByteType::UNSIGNED_64:
    u64Val = byteWrapperPointer->getValueAsU64();
    lua_pushnumber(luaState, static_cast<lua_Number>(u64Val));
    return 1;
  case ByteWrapper::ByteType::SIGNED_64:
    s64Val = byteWrapperPointer->getValueAsS64();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s64Val));
    return 1;
  case ByteWrapper::ByteType::FLOAT:
    floatVal = byteWrapperPointer->getValueAsFloat();
    lua_pushnumber(luaState, static_cast<lua_Number>(floatVal));
    return 1;
  case ByteWrapper::ByteType::DOUBLE:
    doubleVal = byteWrapperPointer->getValueAsDouble();
    lua_pushnumber(luaState, static_cast<lua_Number>(doubleVal));
    return 1;
  default:
    luaL_error(luaState, "Error: argument to get_value() must be a string which is equal to u8, u16, u32, u64, s8, s16, s32, s64, float or double.");
    return 0;
  }
}

int asUnsigned8Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 1)
  {
    luaL_error(luaState, "Error: Cannot return a unsigned_8 from a ByteWrapper which has 0 bytes allocated to it.");
    return 0;
  }
  u8 result = byteWrapperPointer->getValueAsU8();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}

int asU8(lua_State* luaState)
{
  return asUnsigned8Helper(luaState);
}

int asUnsigned8(lua_State* luaState)
{
  return asUnsigned8Helper(luaState);
}

int asUnsignedByte(lua_State* luaState)
{
  return asUnsigned8Helper(luaState);
}

int asSigned8Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 1)
  {
    luaL_error(luaState, "Error: Cannot return a signed_8 from a ByteWrapper which has 0 bytes allocated to it.");
    return 0;
  }
  s8 result = byteWrapperPointer->getValueAsS8();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}

int asS8(lua_State* luaState)
{
  return asSigned8Helper(luaState);
}

int asSigned8(lua_State* luaState)
{
  return asSigned8Helper(luaState);
}

int asSignedByte(lua_State* luaState)
{
  return asSigned8Helper(luaState);
}

int asUnsigned16Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 2)
  {
    luaL_error(luaState, "Error: Cannot return an unsigned_16 from a ByteWrapper which has less than 2 bytes allocated to it.");
    return 0;
  }
  u16 result = byteWrapperPointer->getValueAsU16();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}

int asU16(lua_State* luaState)
{
  return asUnsigned16Helper(luaState);
}

int asUnsigned16(lua_State* luaState)
{
  return asUnsigned16Helper(luaState);
}

int asSigned16Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 2)
  {
    luaL_error(luaState, "Error: Cannot return a signed_16 from a ByteWrapper which has less than 2 bytes allocated to it.");
    return 0;
  }
  s16 result = byteWrapperPointer->getValueAsS16();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}

int asS16(lua_State* luaState)
{
  return asSigned16Helper(luaState);
}

int asSigned16(lua_State* luaState)
{
  return asSigned16Helper(luaState);
}

int asUnsigned32Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 4)
  {
    luaL_error(luaState, "Error: Cannot return an unsigned_32 from a ByteWrapper which has less than 4 bytes allocated to it.");
    return 0;
  }
  u32 result = byteWrapperPointer->getValueAsU32();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}


int asU32(lua_State* luaState)
{
  return asUnsigned32Helper(luaState);
}

int asUnsigned32(lua_State* luaState)
{
  return asUnsigned32Helper(luaState);
}

int asUnsignedInt(lua_State* luaState)
{
  return asUnsigned32Helper(luaState);
}


int asSigned32Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 4)
  {
    luaL_error(luaState, "Error: Cannot return a signed_32 from a ByteWrapper which has less than 4 bytes allocated to it.");
    return 0;
  }
  s32 result = byteWrapperPointer->getValueAsS32();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}

int asS32(lua_State* luaState)
{
  return asSigned32Helper(luaState);
}

int asSigned32(lua_State* luaState)
{
  return asSigned32Helper(luaState);
}

int asSignedInt(lua_State* luaState)
{
  return asSigned32Helper(luaState);
}

int asFloat(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 4)
  {
    luaL_error(luaState, "Error: Cannot return a float from a ByteWrapper which has less than 4 bytes allocated to it.");
    return 0;
  }

  float result = byteWrapperPointer->getValueAsFloat();
  lua_pushnumber(luaState, static_cast<lua_Number>(result));
  return 1;
}

int asUnsigned64Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 8)
  {
    luaL_error(luaState, "Error: Cannot return an unsigned_64 from a ByteWrapper which has less than 8 bytes allocated to it.");
    return 0;
  }

  u64 result = byteWrapperPointer->getValueAsU64();
  lua_pushnumber(luaState, static_cast<lua_Number>(result));
  return 1;
}

int asU64(lua_State* luaState)
{
  return asUnsigned64Helper(luaState);
}

int asUnsigned64(lua_State* luaState)
{
  return asUnsigned64Helper(luaState);
}

int asUnsignedLongLong(lua_State* luaState)
{
  return asUnsigned64Helper(luaState);
}

int asSigned64Helper(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 8)
  {
    luaL_error(luaState, "Error: Cannot return a signed_64 from a ByteWrapper which has less than 8 bytes allocated to it.");
    return 0;
  }

  s64 result = byteWrapperPointer->getValueAsS64();
  lua_pushinteger(luaState, static_cast<lua_Integer>(result));
  return 1;
}

int asS64(lua_State* luaState)
{
  return asSigned64Helper(luaState);
}

int asSigned64(lua_State* luaState)
{
  return asSigned64Helper(luaState);
}

int asSignedLongLong(lua_State* luaState)
{
  return asSigned64Helper(luaState);
}

int asDouble(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer = (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (byteWrapperPointer->totalBytesAllocated < 8)
  {
    luaL_error(luaState, "Error: Cannot return a double from a ByteWrapper which has less than 8 bytes allocated to it.");
    return 0;
  }

  double result = byteWrapperPointer->getValueAsDouble();
  lua_pushnumber(luaState, static_cast<lua_Number>(result));
  return 1;
}

int operatorOverloadedBitwiseAnd(lua_State* luaState)
{
  return -1;
}
/*
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
int operatorOverloadedLessThanEquals(lua_State* luaState);*/

}  // namespace LuaByteWrapper
}  // namespace Lua
