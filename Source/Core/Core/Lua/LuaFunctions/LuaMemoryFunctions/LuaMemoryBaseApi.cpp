#include "LuaMemoryBaseApi.h"

namespace Lua
{

int LuaMemoryBaseApi::do_read_unsigned_8(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32))
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushinteger(luaState, read_u8_from_domain_function(address));
  return 1;
}

 int LuaMemoryBaseApi::do_read_unsigned_16(lua_State* luaState, u16 (*read_u16_from_domain_function)(u32))
{
   u32 address = luaL_checkinteger(luaState, 1);
   lua_pushinteger(luaState, read_u16_from_domain_function(address));
   return 1;
 }

int LuaMemoryBaseApi::do_read_unsigned_32(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32))
 {
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushinteger(luaState, read_u32_from_domain_function(address));
  return 1;
 }

 int LuaMemoryBaseApi::do_read_unsigned_64(lua_State* luaState, u64 (*read_u64_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   lua_pushnumber(luaState, read_u64_from_domain_function(address));
   return 1;
 }

 int LuaMemoryBaseApi::do_read_signed_8(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u8 rawVal = read_u8_from_domain_function(address);
   s8 signedByte = 0;
   memcpy(&signedByte, &rawVal, sizeof(s8));
   lua_pushinteger(luaState, signedByte);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_signed_16(lua_State* luaState, u16 (*read_u16_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u16 rawVal = read_u16_from_domain_function(address);
   s16 signedShort = 0;
   memcpy(&signedShort, &rawVal, sizeof(s16));
   lua_pushinteger(luaState, signedShort);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_signed_32(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u32 rawVal = read_u32_from_domain_function(address);
   s32 signedInt = 0;
   memcpy(&signedInt, &rawVal, sizeof(s32));
   lua_pushinteger(luaState, signedInt);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_signed_64(lua_State* luaState, u64 (*read_u64_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u64 rawVal = read_u64_from_domain_function(address);
   s64 signedLongLong = 0;
   memcpy(&signedLongLong, &rawVal, sizeof(s64));
   lua_pushnumber(luaState, signedLongLong);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_unsigned_byte(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   lua_pushinteger(luaState, read_u8_from_domain_function(address));
   return 1;
 }

 int LuaMemoryBaseApi::do_read_signed_byte(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u8 rawVal = read_u8_from_domain_function(address);
   s8 signedByte = 0;
   memcpy(&signedByte, &rawVal, sizeof(s8));
   lua_pushinteger(luaState, signedByte);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_unsigned_int(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   lua_pushinteger(luaState, read_u32_from_domain_function(address));
   return 1;
 }

 int LuaMemoryBaseApi::do_read_signed_int(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u32 rawVal = read_u32_from_domain_function(address);
   s32 signedInt = 0;
   memcpy(&signedInt, &rawVal, sizeof(s32));
   lua_pushinteger(luaState, signedInt);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_float(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u32 rawVal = read_u32_from_domain_function(address);
   float myFloat = 0.0;
   memcpy(&myFloat, &rawVal, sizeof(float));
   lua_pushnumber(luaState, myFloat);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_double(lua_State* luaState, u64 (*read_u64_from_domain_function)(u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u64 rawVal = read_u64_from_domain_function(address);
   double myDouble = 0.0;
   memcpy(&myDouble, &rawVal, sizeof(double));
   lua_pushnumber(luaState, myDouble);
   return 1;
 }

 int LuaMemoryBaseApi::do_read_fixed_length_string(lua_State* luaState, std::string (*get_string_from_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u32 stringLength = luaL_checkinteger(luaState, 2);
   std::string returnResult = get_string_from_domain_function(address, stringLength);
   lua_pushfstring(luaState, returnResult.c_str());
   return 1;
 }

 int LuaMemoryBaseApi::do_read_null_terminated_string(lua_State* luaState, u8* (*get_pointer_to_domain_function)(u32), std::string (*get_string_from_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u8* memPointerStart = get_pointer_to_domain_function(address);
   u8* memPointerCurrent = memPointerStart;

   while (*memPointerCurrent != '\0')
     memPointerCurrent++;

   std::string returnResult = get_string_from_domain_function(address, (u32)(memPointerCurrent - memPointerStart));
   lua_pushfstring(luaState, returnResult.c_str());
   return 1;
 }

 int LuaMemoryBaseApi::do_write_unsigned_8(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u8 writeVal = luaL_checkinteger(luaState, 2);
   write_u8_to_domain_function(address, writeVal);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_unsigned_16(lua_State* luaState, void (*write_u16_to_domain_function)(u32, u16))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u16 writeVal = luaL_checkinteger(luaState, 2);
   write_u16_to_domain_function(address, writeVal);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_unsigned_32(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u32 writeVal = luaL_checkinteger(luaState, 2);
   write_u32_to_domain_function(address, writeVal);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_unsigned_64(lua_State* luaState, void (*write_u64_to_domain_function)(u32, u64))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u64 writeVal = luaL_checknumber(luaState, 2);
   write_u64_to_domain_function(address, writeVal);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_signed_8(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   s8 rawVal = luaL_checkinteger(luaState, 2);
   u8 convertedByte = 0;
   memcpy(&convertedByte, &rawVal, sizeof(u8));
   write_u8_to_domain_function(address, convertedByte);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_signed_16(lua_State* luaState, void (*write_u16_to_domain_function)(u32, u16))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   s16 rawVal = luaL_checkinteger(luaState, 2);
   u16 convertedShort = 0;
   memcpy(&convertedShort, &rawVal, sizeof(u16));
   write_u16_to_domain_function(address, convertedShort);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_signed_32(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   s32 rawVal = luaL_checkinteger(luaState, 2);
   u32 convertedInt = 0;
   memcpy(&convertedInt, &rawVal, sizeof(u32));
   write_u32_to_domain_function(address, convertedInt);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_signed_64(lua_State* luaState, void (*write_u64_to_domain_function)(u32, u64))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   s64 rawVal = luaL_checknumber(luaState, 2);
   u64 convertedLongLong = 0;
   memcpy(&convertedLongLong, &rawVal, sizeof(u64));
   write_u64_to_domain_function(address, convertedLongLong);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_unsigned_byte(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u8 rawVal = luaL_checkinteger(luaState, 2);
   write_u8_to_domain_function(address, rawVal);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_signed_byte(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   s8 rawVal = luaL_checkinteger(luaState, 2);
   u8 convertedByte = 0;
   memcpy(&convertedByte, &rawVal, sizeof(u8));
   write_u8_to_domain_function(address, convertedByte);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_unsigned_int(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   u32 rawVal = luaL_checkinteger(luaState, 2);
   write_u32_to_domain_function(address, rawVal);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_signed_int(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   s32 rawVal = luaL_checkinteger(luaState, 2);
   u32 convertedInt = 0;
   memcpy(&convertedInt, &rawVal, sizeof(u32));
   write_u32_to_domain_function(address, convertedInt);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_float(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   float rawVal = luaL_checknumber(luaState, 2);
   u32 convertedInt = 0;
   memcpy(&convertedInt, &rawVal, sizeof(u32));
   write_u32_to_domain_function(address, convertedInt);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_double(lua_State* luaState, void (*write_u64_to_domain_function)(u32, u64))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   double rawVal = luaL_checknumber(luaState, 2);
   u64 convertedLongLong = 0;
   memcpy(&convertedLongLong, &rawVal, sizeof(u64));
   write_u64_to_domain_function(address, convertedLongLong);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_fixed_length_string(lua_State* luaState, void (*copy_string_to_domain_function)(u32, const char*, size_t))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   const char* stringToWrite = luaL_checkstring(luaState, 2);
   u32 stringSize = luaL_checkinteger(luaState, 3);
   copy_string_to_domain_function(address, stringToWrite, stringSize);
   return 0;
 }

 int LuaMemoryBaseApi::do_write_null_terminated_string(lua_State* luaState, void (*copy_string_to_domain_function)(u32, const char*, size_t))
 {
   u32 address = luaL_checkinteger(luaState, 1);
   const char* stringToWrite = luaL_checkstring(luaState, 2);
   size_t stringSize = std::strlen(stringToWrite);
   copy_string_to_domain_function(address, stringToWrite, stringSize);
   return 0;
 }
}  // namespace Lua
