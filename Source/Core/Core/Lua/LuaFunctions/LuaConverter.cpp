#include "LuaConverter.h"
#include <type_traits>
namespace Lua
{
namespace LuaConverter
{

static u8 u8Type;
static u16 u16Type;
static u32 u32Type;
static u64 u64Type;
static s8 s8Type;
static s16 s16Type;
static s32 s32Type;
static s64 s64Type;
static float floatType;
static double doubleType;

Converter* GetConverterInstance()
{
  if (converterInstance == NULL)
    converterInstance = new Converter();
  return converterInstance;
}

void InitLuaConverterFunctions(lua_State* luaState)
{
  Converter** converterPtrPtr = (Converter**)lua_newuserdata(luaState, sizeof(Converter*));
  *converterPtrPtr = GetConverterInstance();
  luaL_newmetatable(luaState, "LuaConverterMetaTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaConverterFunctions[] = {

      {"convert_signed_8_to_unsigned_8", convert_signed_8_to_unsigned_8},
      {"convert_unsigned_8_to_signed_8", convert_unsigned_8_to_signed_8},
      {"convert_signed_byte_to_unsigned_byte", convert_signed_byte_to_unsigned_byte},
      {"convert_unsigned_byte_to_signed_byte", convert_unsigned_byte_to_signed_byte},
      {"convert_signed_16_to_unsigned_16", convert_signed_16_to_unsigned_16},
      {"convert_unsigned_16_to_signed_16", convert_unsigned_16_to_signed_16},
      {"convert_unsigned_32_to_signed_32", convert_unsigned_32_to_signed_32},
      {"convert_signed_32_to_unsigned_32", convert_signed_32_to_unsigned_32},
      {"convert_unsigned_int_to_signed_int", convert_unsigned_int_to_signed_int},
      {"convert_signed_int_to_unsigned_int", convert_signed_int_to_unsigned_int},
      {"convert_unsigned_32_to_float", convert_unsigned_32_to_float},
      {"convert_signed_32_to_float", convert_signed_32_to_float},
      {"convert_unsigned_int_to_float", convert_unsigned_int_to_float},
      {"convert_signed_int_to_float", convert_signed_int_to_float},
      {"convert_float_to_unsigned_32", convert_float_to_unsigned_32},
      {"convert_float_to_signed_32", convert_float_to_signed_32},
      {"convert_float_to_unsigned_int", convert_float_to_unsigned_int},
      {"convert_float_to_signed_int", convert_float_to_signed_int},
      {"convert_unsigned_64_to_signed_64", convert_unsigned_64_to_signed_64},
      {"convert_signed_64_to_unsigned_64", convert_signed_64_to_unsigned_64},
      {"convert_unsigned_64_to_double", convert_unsigned_64_to_double},
      {"convert_signed_64_to_double", convert_signed_64_to_double},
      {"convert_double_to_unsigned_64", convert_double_to_unsigned_64},
      {"convert_double_to_signed_64", convert_double_to_signed_64},
      {nullptr, nullptr}};

  luaL_setfuncs(luaState, luaConverterFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "converter");
}
  template<typename T, typename V>
int conversion_helper_function(lua_State* luaState, T inputType, V outputType)
{
  T inputValue = 0;
  if (std::is_integral<T>() && sizeof(inputType) < 8)
    inputValue = luaL_checkinteger(luaState, 2);
  else
    inputValue = luaL_checknumber(luaState, 2);
  V outputValue = 0;
  memcpy(&outputValue, &inputValue, sizeof(outputValue));
  if (std::is_integral<V>() && sizeof(outputType) < 8)
    lua_pushinteger(luaState, outputValue);
  else
    lua_pushnumber(luaState, outputValue);
  return 1;
}


int convert_signed_8_to_unsigned_8(lua_State* luaState)
{
  return conversion_helper_function(luaState, s8Type, u8Type);
}

int convert_unsigned_8_to_signed_8(lua_State* luaState)
{
  return conversion_helper_function(luaState, u8Type, s8Type);
}

int convert_signed_byte_to_unsigned_byte(lua_State* luaState)
{
  return conversion_helper_function(luaState, s8Type, u8Type);
}

int convert_unsigned_byte_to_signed_byte(lua_State* luaState)
{
  return conversion_helper_function(luaState, u8Type, s8Type);
}

int convert_signed_16_to_unsigned_16(lua_State* luaState)
{
  return conversion_helper_function(luaState, s16Type, u16Type);
}

int convert_unsigned_16_to_signed_16(lua_State* luaState)
{
  return conversion_helper_function(luaState, u16Type, s16Type);
}

int convert_unsigned_32_to_signed_32(lua_State* luaState)
{
  return conversion_helper_function(luaState, u32Type, s32Type);
}

int convert_signed_32_to_unsigned_32(lua_State* luaState)
{
  return conversion_helper_function(luaState, s32Type, u32Type);
}

int convert_unsigned_int_to_signed_int(lua_State* luaState)
{
  return conversion_helper_function(luaState, u32Type, s32Type);
}

int convert_signed_int_to_unsigned_int(lua_State* luaState)
{
  return conversion_helper_function(luaState, s32Type, u32Type);
}

int convert_unsigned_32_to_float(lua_State* luaState)
{
  return conversion_helper_function(luaState, u32Type, floatType);
}

int convert_signed_32_to_float(lua_State* luaState)
{
  return conversion_helper_function(luaState, s32Type, floatType);
}

int convert_unsigned_int_to_float(lua_State* luaState)
{
  return conversion_helper_function(luaState, u32Type, floatType);
}

int convert_signed_int_to_float(lua_State* luaState)
{
  return conversion_helper_function(luaState, s32Type, floatType);
}

int convert_float_to_unsigned_32(lua_State* luaState)
{
  return conversion_helper_function(luaState, floatType, u32Type);
}

int convert_float_to_signed_32(lua_State* luaState)
{
  return conversion_helper_function(luaState, floatType, s32Type);
}

int convert_float_to_unsigned_int(lua_State* luaState)
{
  return conversion_helper_function(luaState, floatType, u32Type);
}

int convert_float_to_signed_int(lua_State* luaState)
{
  return conversion_helper_function(luaState, floatType, s32Type);
}

int convert_unsigned_64_to_signed_64(lua_State* luaState)
{
  return conversion_helper_function(luaState, u64Type, s64Type);
}

int convert_signed_64_to_unsigned_64(lua_State* luaState)
{
  return conversion_helper_function(luaState, s64Type, u64Type);
}

int convert_unsigned_64_to_double(lua_State* luaState)
{
  return conversion_helper_function(luaState, u64Type, doubleType);
}

int convert_signed_64_to_double(lua_State* luaState)
{
  return conversion_helper_function(luaState, s64Type, doubleType);
}

int convert_double_to_unsigned_64(lua_State* luaState)
{
  return conversion_helper_function(luaState, doubleType, u64Type);
}

int convert_double_to_signed_64(lua_State* luaState)
{
  return conversion_helper_function(luaState, doubleType, s64Type);
}

}
}
