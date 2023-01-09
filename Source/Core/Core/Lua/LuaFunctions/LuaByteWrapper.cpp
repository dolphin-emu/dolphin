#include "LuaByteWrapper.h"
#include <format>
#include "common/Swap.h"
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

  lua_pushcfunction(luaState, withType);
  lua_setfield(luaState, -2, "with_type");

  lua_pushcfunction(luaState, getValue);
  lua_setfield(luaState, -2, "get_value");

  lua_pushcfunction(luaState, operatorOverloadedBitwiseAnd);
  lua_setfield(luaState, -2, "__band");

  lua_pushcfunction(luaState, operatorOverloadedBitwiseOr);
  lua_setfield(luaState, -2, "__bor");

  lua_pushcfunction(luaState, operatorOverloadedBitwiseXor);
  lua_setfield(luaState, -2, "__bxor");

  lua_pushcfunction(luaState, operatorOverloadedBitwiseNot);
  lua_setfield(luaState, -2, "__bnot");

  lua_pushcfunction(luaState, operatorOverloadedBitShiftLeft);
  lua_setfield(luaState, -2, "__shl");

  lua_pushcfunction(luaState, operatorOverloadedBitShiftRight);
  lua_setfield(luaState, -2, "__shr");

  lua_pushcfunction(luaState, operatorOverloadedEqualsEquals);
  lua_setfield(luaState, -2, "__eq");

  lua_pushcfunction(luaState, operatorOverloadedLessThan);
  lua_setfield(luaState, -2, "__lt");

  lua_pushcfunction(luaState, operatorOverloadedLessThanEquals);
  lua_setfield(luaState, -2, "__le");

  lua_pop(luaState, 1);
}

int newByteWrapper(lua_State* luaState)
{
  if (lua_isinteger(luaState, 1))
  {
    long long longLongInitialValue = luaL_checkinteger(luaState, 1);
    u64 convertedU64 = 0ULL;
    memcpy(&convertedU64, &longLongInitialValue, sizeof(u64));
    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
        ByteWrapper::CreateByteWrapperFromU64(convertedU64);
  }
  else if (lua_isnumber(luaState, 1))
  {
    double doubleInitialValue = luaL_checknumber(luaState, 1);
    u64 doubleToU64 = 0ULL;
    memcpy(&doubleToU64, &doubleInitialValue, sizeof(u64));
    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
        ByteWrapper::CreateByteWrapperFromU64(doubleToU64);
  }
  else if (lua_isuserdata(luaState, 1))
  {
    ByteWrapper* byteWrapperPointer =
        (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
        ByteWrapper::CreateByteWrapperFromCopy(byteWrapperPointer);
  }
  else
  {
    luaL_error(luaState, "Error: Constructor for ByteWrapper() was passed an invalid argument. "
                         "Valid arguments are integer, number or another ByteWrapper.");
    return 1;
  }

  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int deleteByteWrapper(lua_State* luaState)
{
  //delete *reinterpret_cast<ByteWrapper**>(lua_touserdata(luaState, 1));
  return 0;
}

int withType(lua_State* luaState)
{
  ByteWrapper* byteWrapperPointer =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  const char* typeString = luaL_checkstring(luaState, 2);
  NumberType parsedType = parseType(typeString);
  if (parsedType == NumberType::UNDEFINED)
  {
    luaL_error(luaState, "Error: Argument to withType() must be a string which such as u8, s8, "
                         "u16, s16, u32, s32, float, u64, s64, or double.");
    return 0;
  }
  else if (!ByteWrapper::typeSizeCheck(byteWrapperPointer, parsedType))
  {
    return 0;
  }

  byteWrapperPointer->setType(parsedType);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
      byteWrapperPointer;
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int getValue(lua_State* luaState)
{
  bool noStringArgument = false;
  ByteWrapper* byteWrapperPointer =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  NumberType typeArgument = NumberType::UNDEFINED;
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

  if (lua_gettop(luaState) < 2)  // case where no argument for type was passed - meaning we should
                                 // return the value in the type of the ByteWrapper
  {
    noStringArgument = true;
    typeArgument = byteWrapperPointer->byteType;
  }
  else
  {
    const char* typeString = luaL_checkstring(luaState, 2);
    if (typeString == NULL)
    {
      luaL_error(luaState, "Error: argument to get_value() must be a string such as u8, "
                           "u16, u32, u64, s8, s16, s32, s64, float or double.");
      return 0;
    }
    typeArgument = parseType(typeString);
  }

  if (typeArgument == NumberType::UNDEFINED)
  {
    if (noStringArgument)
    {
      luaL_error(luaState, "Error: get_value() was called on a ByteWrapper without first setting "
                           "the type of the ByteWrapper using the with_type() function.");
    }
    else
    {
      luaL_error(luaState, "Error: argument to get_value() must be a string such as u8, "
                           "u16, u32, u64, s8, s16, s32, s64, float or double.");
    }
    return 0;
  }

  switch (typeArgument)
  {
  case NumberType::UNSIGNED_8:
    u8Val = byteWrapperPointer->getValueAsU8();
    lua_pushinteger(luaState, static_cast<lua_Integer>(u8Val));
    return 1;
  case NumberType::SIGNED_8:
    s8Val = byteWrapperPointer->getValueAsS8();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s8Val));
    return 1;
  case NumberType::UNSIGNED_16:
    u16Val = byteWrapperPointer->getValueAsU16();
    lua_pushinteger(luaState, static_cast<lua_Integer>(u16Val));
    return 1;
  case NumberType::SIGNED_16:
    s16Val = byteWrapperPointer->getValueAsS16();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s16Val));
    return 1;
  case NumberType::UNSIGNED_32:
    u32Val = byteWrapperPointer->getValueAsU32();
    lua_pushinteger(luaState, static_cast<lua_Integer>(u32Val));
    return 1;
  case NumberType::SIGNED_32:
    s32Val = byteWrapperPointer->getValueAsS32();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s32Val));
    return 1;
  case NumberType::UNSIGNED_64:
    u64Val = byteWrapperPointer->getValueAsU64();
    lua_pushnumber(luaState, static_cast<lua_Number>(u64Val));
    return 1;
  case NumberType::SIGNED_64:
    s64Val = byteWrapperPointer->getValueAsS64();
    lua_pushinteger(luaState, static_cast<lua_Integer>(s64Val));
    return 1;
  case NumberType::FLOAT:
    floatVal = byteWrapperPointer->getValueAsFloat();
    lua_pushnumber(luaState, static_cast<lua_Number>(floatVal));
    return 1;
  case NumberType::DOUBLE:
    doubleVal = byteWrapperPointer->getValueAsDouble();
    lua_pushnumber(luaState, static_cast<lua_Number>(doubleVal));
    return 1;
  default:
    luaL_error(luaState, "Error: argument to get_value() must be a string such as u8, "
                         "u16, u32, u64, s8, s16, s32, s64, float or double.");
    return 0;
  }
}

int operatorOverloadNonComparisonCommonFunc(lua_State* luaState, ByteWrapper::OPERATIONS operation,
                                            const char* functionName)
{
  ByteWrapper* firstByteWrapper =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (firstByteWrapper->getByteSize() <= 0)
  {
    luaL_error(luaState, (std::string("Error: Cannot perform ") + functionName +
                          " on a ByteWrapper with a size of 0")
                             .c_str());
    return 0;
  }

  ByteWrapper* secondByteWrapper = NULL;
  if (lua_isinteger(luaState, 2))
    secondByteWrapper = ByteWrapper::CreateByteWrapperFromLongLong(luaL_checkinteger(luaState, 2));

  else if (lua_isnumber(luaState, 2))
    secondByteWrapper = ByteWrapper::CreateByteWrapperFromDouble(luaL_checknumber(luaState, 2));

  else if (lua_isuserdata(luaState, 2))
    secondByteWrapper =
        (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 2, LUA_BYTE_WRAPPER)));

  else
  {
    luaL_error(luaState, (std::string("Error: Invalid argument passed into ") + functionName +
                          ". Second argument must be integer, number, or another ByteWrapper")
                             .c_str());
    return 0;
  }

  ByteWrapper tempResult =
      firstByteWrapper->doNonComparisonOperation(*secondByteWrapper, operation);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
      ByteWrapper::CreateByteWrapperFromCopy(&tempResult);
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  delete secondByteWrapper;
  return 1;
}

int operatorOverloadedBitwiseAnd(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::BITWISE_AND, "Bitwise And");
}

int operatorOverloadedBitwiseOr(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::BITWISE_OR, "Bitwise Or");
}

int operatorOverloadedBitwiseXor(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::BITWISE_XOR, "Bitwise XOR");
}

int operatorOverloadedLogicalAnd(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::LOGICAL_AND, "Logical And");
}

int operatorOverloadedLogicalOr(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::LOGICAL_OR, "Logical Or");
}

int operatorOverloadedBitShiftLeft(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::BITSHIFT_LEFT,
                                                 "BitShift Left");
}

int operatorOverloadedBitShiftRight(lua_State* luaState)
{
  return operatorOverloadNonComparisonCommonFunc(luaState, ByteWrapper::BITSHIFT_RIGHT,
                                                 "BitShift Right");
}

int operatorOverloadedBitwiseNot(lua_State* luaState)
{
  ByteWrapper* inputByteWrapper =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  ByteWrapper tempResult = ~(*inputByteWrapper);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
      ByteWrapper::CreateByteWrapperFromCopy(&tempResult);
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int operatorOverloadedLogicalNot(lua_State* luaState)
{
  ByteWrapper* inputByteWrapper =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  ByteWrapper tempResult = !(*inputByteWrapper);
  *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) =
      ByteWrapper::CreateByteWrapperFromCopy(&tempResult);
  luaL_setmetatable(luaState, LUA_BYTE_WRAPPER);
  return 1;
}

int operatorOverloadComparisonCommonFunc(lua_State* luaState, ByteWrapper::OPERATIONS operation,
                                         const char* functionName)
{
  ByteWrapper* firstByteWrapper =
      (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 1, LUA_BYTE_WRAPPER)));
  if (firstByteWrapper->getByteSize() <= 0)
  {
    luaL_error(luaState, (std::string("Error: Cannot perform ") + functionName +
                          " on a ByteWrapper with a size of 0")
                             .c_str());
    return 0;
  }

  ByteWrapper* secondByteWrapper = NULL;
  if (lua_isinteger(luaState, 2))
    secondByteWrapper = ByteWrapper::CreateByteWrapperFromLongLong(luaL_checkinteger(luaState, 2));

  else if (lua_isnumber(luaState, 2))
    secondByteWrapper = ByteWrapper::CreateByteWrapperFromDouble(luaL_checknumber(luaState, 2));

  else if (lua_isuserdata(luaState, 2))
    secondByteWrapper =
        (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 2, LUA_BYTE_WRAPPER)));

  else
  {
    luaL_error(luaState, (std::string("Error: Invalid argument passed into ") + functionName +
                          ". Second argument must be integer, number, or another ByteWrapper")
                             .c_str());
    return 0;
  }

  bool result = firstByteWrapper->doComparisonOperation(*secondByteWrapper, operation);
  lua_pushboolean(luaState, result);
  return 1;
}

int operatorOverloadedEqualsEquals(lua_State* luaState)
{
  return operatorOverloadComparisonCommonFunc(luaState, ByteWrapper::EQUALS_EQUALS, "==");
}

int operatorOverloadedLessThan(lua_State* luaState)
{
  return operatorOverloadComparisonCommonFunc(luaState, ByteWrapper::LESS_THAN, "<");
}

int operatorOverloadedLessThanEquals(lua_State* luaState)
{
  return operatorOverloadComparisonCommonFunc(luaState, ByteWrapper::LESS_THAN_EQUALS, "<=");
}

}  // namespace LuaByteWrapper
}  // namespace Lua
