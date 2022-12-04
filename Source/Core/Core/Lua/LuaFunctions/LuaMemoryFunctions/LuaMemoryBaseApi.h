#pragma once
#include <string>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "common/CommonTypes.h"

namespace Lua
{
  template<typename T>
class LuaMemoryBaseApi
{
public:
  inline static u8 (*read_u8_from_domain_function)(lua_State*, u32) = NULL;
  inline static u16 (*read_u16_from_domain_function)(lua_State*, u32) = NULL;
  inline static u32 (*read_u32_from_domain_function)(lua_State*, u32) = NULL;
  inline static u64 (*read_u64_from_domain_function)(lua_State*, u32) = NULL;
  inline static std::string (*get_string_from_domain_function)(lua_State*, u32, u32) = NULL;
  inline static u8* (*get_pointer_to_domain_function)(lua_State*, u32) = NULL;
  inline static void (*write_u8_to_domain_function)(lua_State*, u32, u8) = NULL;
  inline static void (*write_u16_to_domain_function)(lua_State*, u32, u16) = NULL;
  inline static void (*write_u32_to_domain_function)(lua_State*, u32, u32) = NULL;
  inline static void (*write_u64_to_domain_function)(lua_State*, u32, u64) = NULL;
  inline static void (*copy_string_to_domain_function)(lua_State*, u32, const char*, size_t) = NULL;

  inline static T* instance = NULL;

  static T* GetInstance();

  static void LuaMemoryBaseInit(lua_State* luaState, const char* domainName, luaL_Reg* luaFunctionsList);
  static int do_read_unsigned_8(lua_State* luaState);
  static int do_read_unsigned_16(lua_State* luaState);
  static int do_read_unsigned_32(lua_State* luaState);
  static int do_read_unsigned_64(lua_State* luaState);

  static int do_read_signed_8(lua_State* luaState);
  static int do_read_signed_16(lua_State* luaState);
  static int do_read_signed_32(lua_State* luaState);
  static int do_read_signed_64(lua_State* luaState);

  static int do_read_unsigned_byte(lua_State* luaState);
  static int do_read_signed_byte(lua_State* luaState);

  static int do_read_unsigned_int(lua_State* luaState);
  static int do_read_signed_int(lua_State* luaState);

  static int do_read_float(lua_State* luaState);
  static int do_read_double(lua_State* luaState);

  static int do_read_fixed_length_string(lua_State* luaState);
  static int do_read_null_terminated_string(lua_State* luaState);

  static int do_read_unsigned_bytes(lua_State* luaState);
  static int do_read_signed_bytes(lua_State* luaState);

  static int do_write_unsigned_8(lua_State* luaState);
  static int do_write_unsigned_16(lua_State* luaState);
  static int do_write_unsigned_32(lua_State* luaState);
  static int do_write_unsigned_64(lua_State* luaState);

  static int do_write_signed_8(lua_State* luaState);
  static int do_write_signed_16(lua_State* luaState);
  static int do_write_signed_32(lua_State* luaState);
  static int do_write_signed_64(lua_State* luaState);

  static int do_write_unsigned_byte(lua_State* luaState);
  static int do_write_signed_byte(lua_State* luaState);

  static int do_write_unsigned_int(lua_State* luaState);
  static int do_write_signed_int(lua_State* luaState);

  static int do_write_float(lua_State* luaState);
  static int do_write_double(lua_State* luaState);

  static int do_write_fixed_length_string(lua_State* luaState);
  static int do_write_null_terminated_string(lua_State* luaState);
};

template <typename T>
T* LuaMemoryBaseApi<T>::GetInstance()
{
  if (instance == NULL)
    instance = new T();
  return instance;
}

template <typename T>
void LuaMemoryBaseApi<T>::LuaMemoryBaseInit(lua_State* luaState, const char* domainName,
                                                   luaL_Reg* luaFunctionsList)
{
  T** instancePtrPtr = (T**)lua_newuserdata(luaState, sizeof(T*));
  *instancePtrPtr = GetInstance();
  luaL_newmetatable(luaState, (std::string("Lua") + domainName + "MetaTable").c_str());
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");
  luaL_setfuncs(luaState, luaFunctionsList, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, domainName);
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  lua_pushinteger(luaState, read_u8_from_domain_function(luaState, address));
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  lua_pushinteger(luaState, read_u16_from_domain_function(luaState, address));
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  lua_pushinteger(luaState, read_u32_from_domain_function(luaState, address));
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  lua_pushnumber(luaState, read_u64_from_domain_function(luaState, address));
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u8 rawVal = read_u8_from_domain_function(luaState, address);
  s8 signedByte = 0;
  memcpy(&signedByte, &rawVal, sizeof(s8));
  lua_pushinteger(luaState, signedByte);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u16 rawVal = read_u16_from_domain_function(luaState, address);
  s16 signedShort = 0;
  memcpy(&signedShort, &rawVal, sizeof(s16));
  lua_pushinteger(luaState, signedShort);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 rawVal = read_u32_from_domain_function(luaState, address);
  s32 signedInt = 0;
  memcpy(&signedInt, &rawVal, sizeof(s32));
  lua_pushinteger(luaState, signedInt);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u64 rawVal = read_u64_from_domain_function(luaState, address);
  s64 signedLongLong = 0;
  memcpy(&signedLongLong, &rawVal, sizeof(s64));
  lua_pushnumber(luaState, signedLongLong);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  lua_pushinteger(luaState, read_u8_from_domain_function(luaState, address));
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u8 rawVal = read_u8_from_domain_function(luaState, address);
  s8 signedByte = 0;
  memcpy(&signedByte, &rawVal, sizeof(s8));
  lua_pushinteger(luaState, signedByte);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  lua_pushinteger(luaState, read_u32_from_domain_function(luaState, address));
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 rawVal = read_u32_from_domain_function(luaState, address);
  s32 signedInt = 0;
  memcpy(&signedInt, &rawVal, sizeof(s32));
  lua_pushinteger(luaState, signedInt);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_float(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 rawVal = read_u32_from_domain_function(luaState, address);
  float myFloat = 0.0;
  memcpy(&myFloat, &rawVal, sizeof(float));
  lua_pushnumber(luaState, myFloat);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_double(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u64 rawVal = read_u64_from_domain_function(luaState, address);
  double myDouble = 0.0;
  memcpy(&myDouble, &rawVal, sizeof(double));
  lua_pushnumber(luaState, myDouble);
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_fixed_length_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 stringLength = luaL_checkinteger(luaState, 3);
  std::string returnResult = get_string_from_domain_function(luaState, address, stringLength);
  lua_pushfstring(luaState, returnResult.c_str());
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_null_terminated_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u8* memPointerStart = get_pointer_to_domain_function(luaState, address);
  u8* memPointerCurrent = memPointerStart;

  while (*memPointerCurrent != '\0')
    memPointerCurrent++;

  std::string returnResult =
      get_string_from_domain_function(luaState, address, (u32)(memPointerCurrent - memPointerStart));
  lua_pushfstring(luaState, returnResult.c_str());
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_unsigned_bytes(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 numBytes = luaL_checkinteger(luaState, 3);
  u8* pointerToBaseAddress = get_pointer_to_domain_function(luaState, address);
  lua_createtable(luaState, numBytes, 0);
  for (size_t i = 0; i < numBytes; ++i)
  {
    u8 currByte = pointerToBaseAddress[i];
    lua_pushinteger(luaState, static_cast<lua_Integer>(currByte));
    lua_rawseti(luaState, -2, i + 1);
  }
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_read_signed_bytes(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 numBytes = luaL_checkinteger(luaState, 3);
  u8* pointerToBaseAddress = get_pointer_to_domain_function(luaState, address);
  lua_createtable(luaState, numBytes, 0);
  for (size_t i = 0; i < numBytes; ++i)
  {
    s8 currByteSigned = 0;
    memcpy(&currByteSigned, pointerToBaseAddress + i, sizeof(s8));
    lua_pushinteger(luaState, static_cast<lua_Integer>(currByteSigned));
    lua_rawseti(luaState, -2, i + 1);
  }
  return 1;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_unsigned_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u8 writeVal = luaL_checkinteger(luaState, 3);
  write_u8_to_domain_function(luaState, address, writeVal);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_unsigned_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u16 writeVal = luaL_checkinteger(luaState, 3);
  write_u16_to_domain_function(luaState, address, writeVal);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_unsigned_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 writeVal = luaL_checkinteger(luaState, 3);
  write_u32_to_domain_function(luaState, address, writeVal);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_unsigned_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u64 writeVal = luaL_checknumber(luaState, 3);
  write_u64_to_domain_function(luaState, address, writeVal);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_signed_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  s8 rawVal = luaL_checkinteger(luaState, 3);
  u8 convertedByte = 0;
  memcpy(&convertedByte, &rawVal, sizeof(u8));
  write_u8_to_domain_function(luaState, address, convertedByte);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_signed_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  s16 rawVal = luaL_checkinteger(luaState, 3);
  u16 convertedShort = 0;
  memcpy(&convertedShort, &rawVal, sizeof(u16));
  write_u16_to_domain_function(luaState, address, convertedShort);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_signed_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  s32 rawVal = luaL_checkinteger(luaState, 3);
  u32 convertedInt = 0;
  memcpy(&convertedInt, &rawVal, sizeof(u32));
  write_u32_to_domain_function(luaState, address, convertedInt);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_signed_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  s64 rawVal = luaL_checknumber(luaState, 3);
  u64 convertedLongLong = 0;
  memcpy(&convertedLongLong, &rawVal, sizeof(u64));
  write_u64_to_domain_function(luaState, address, convertedLongLong);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_unsigned_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u8 rawVal = luaL_checkinteger(luaState, 3);
  write_u8_to_domain_function(luaState, address, rawVal);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_signed_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  s8 rawVal = luaL_checkinteger(luaState, 3);
  u8 convertedByte = 0;
  memcpy(&convertedByte, &rawVal, sizeof(u8));
  write_u8_to_domain_function(luaState, address, convertedByte);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_unsigned_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  u32 rawVal = luaL_checkinteger(luaState, 3);
  write_u32_to_domain_function(luaState, address, rawVal);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_signed_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  s32 rawVal = luaL_checkinteger(luaState, 3);
  u32 convertedInt = 0;
  memcpy(&convertedInt, &rawVal, sizeof(u32));
  write_u32_to_domain_function(luaState, address, convertedInt);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_float(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  float rawVal = luaL_checknumber(luaState, 3);
  u32 convertedInt = 0;
  memcpy(&convertedInt, &rawVal, sizeof(u32));
  write_u32_to_domain_function(luaState, address, convertedInt);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_double(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  double rawVal = luaL_checknumber(luaState, 3);
  u64 convertedLongLong = 0;
  memcpy(&convertedLongLong, &rawVal, sizeof(u64));
  write_u64_to_domain_function(luaState, address, convertedLongLong);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_fixed_length_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  const char* stringToWrite = luaL_checkstring(luaState, 3);
  u32 stringSize = luaL_checkinteger(luaState, 4);
  copy_string_to_domain_function(luaState, address, stringToWrite, stringSize);
  return 0;
}

template <typename T>
int LuaMemoryBaseApi<T>::do_write_null_terminated_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 2);
  const char* stringToWrite = luaL_checkstring(luaState, 3);
  size_t stringSize = std::strlen(stringToWrite);
  copy_string_to_domain_function(luaState, address, stringToWrite, stringSize);
  return 0;
}
}  // namespace Lua
