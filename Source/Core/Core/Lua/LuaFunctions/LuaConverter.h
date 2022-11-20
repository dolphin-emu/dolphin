#pragma once
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "common/CommonTypes.h"

namespace Lua
{
  //These functions copy the binary representation of one number directly into another type, and then return the result.
  //For example, if we call convert_signed_u8_to_unsigned_u8() with 255 as an argument, then -1 will be returned.
  //This is because 255 in binary (as an unsignd byte) is 1111 1111, and this number is -1 if viewed as a signed byte.
namespace LuaConverter
{
class Converter
{
public:
  Converter() {}
};

static Converter* converterInstance;

Converter* GetConverterInstance();
void InitLuaConverterFunctions(lua_State* luaState);

  int convert_signed_8_to_unsigned_8(lua_State* luaState);
  int convert_unsigned_8_to_signed_8(lua_State* luaState);
  int convert_signed_byte_to_unsigned_byte(lua_State* luaState);
  int convert_unsigned_byte_to_signed_byte(lua_State* luaState);

  int convert_signed_16_to_unsigned_16(lua_State* luaState);
  int convert_unsigned_16_to_signed_16(lua_State* luaState);

  int convert_unsigned_32_to_signed_32(lua_State* luaState);
  int convert_signed_32_to_unsigned_32(lua_State* luaState);
  int convert_unsigned_int_to_signed_int(lua_State* luaState);
  int convert_signed_int_to_unsigned_int(lua_State* luaState);
  int convert_unsigned_32_to_float(lua_State* luaState);
  int convert_signed_32_to_float(lua_State* luaState);
  int convert_unsigned_int_to_float(lua_State* luaState);
  int convert_signed_int_to_float(lua_State* luaState);
  int convert_float_to_unsigned_32(lua_State* luaState);
  int convert_float_to_signed_32(lua_State* luaState);
  int convert_float_to_unsigned_int(lua_State* luaState);
  int convert_float_to_signed_int(lua_State* luaState);

  int convert_unsigned_64_to_signed_64(lua_State* luaState);
  int convert_signed_64_to_unsigned_64(lua_State* luaState);
  int convert_unsigned_64_to_double(lua_State* luaState);
  int convert_signed_64_to_double(lua_State* luaState);
  int convert_double_to_unsigned_64(lua_State* luaState);
  int convert_double_to_signed_64(lua_State* luaState);
}
}
