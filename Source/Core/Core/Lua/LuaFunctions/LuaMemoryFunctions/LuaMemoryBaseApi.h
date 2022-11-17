#pragma once
#include <string>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "common/CommonTypes.h"

namespace Lua
{
class LuaMemoryBaseApi
{
public:

  static int do_read_unsigned_8(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32));
  static int do_read_unsigned_16(lua_State* luaState, u16 (*read_u16_from_domain_function)(u32));
  static int do_read_unsigned_32(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32));
  static int do_read_unsigned_64(lua_State* luaState, u64 (*read_u64_from_domain_function)(u32));

  static int do_read_signed_8(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32));
  static int do_read_signed_16(lua_State* luaState, u16 (*read_u16_from_domain_function)(u32));
  static int do_read_signed_32(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32));
  static int do_read_signed_64(lua_State* luaState, u64 (*read_u64_from_domain_function)(u32));

  static int do_read_unsigned_byte(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32));
  static int do_read_signed_byte(lua_State* luaState, u8 (*read_u8_from_domain_function)(u32));

  static int do_read_unsigned_int(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32));
  static int do_read_signed_int(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32));

  static int do_read_float(lua_State* luaState, u32 (*read_u32_from_domain_function)(u32));
  static int do_read_double(lua_State* luaState, u64 (*read_u64_from_domain_function)(u32));

  static int do_read_fixed_length_string(lua_State* luaState, std::string (*get_string_from_domain_function)(u32, u32));
  static int do_read_null_terminated_string(lua_State* luaState, u8* (*get_pointer_to_domain_function)(u32), std::string (*get_string_from_domain_function)(u32, u32));


  static int do_write_unsigned_8(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8));
  static int do_write_unsigned_16(lua_State* luaState, void (*write_u16_to_domain_function)(u32, u16));
  static int do_write_unsigned_32(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32));
  static int do_write_unsigned_64(lua_State* luaState, void (*write_u64_to_domain_function)(u32, u64));

  static int do_write_signed_8(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8));
  static int do_write_signed_16(lua_State* luaState, void (*write_u16_to_domain_function)(u32, u16));
  static int do_write_signed_32(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32));
  static int do_write_signed_64(lua_State* luaState, void (*write_u64_to_domain_function)(u32, u64));

  static int do_write_unsigned_byte(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8));
  static int do_write_signed_byte(lua_State* luaState, void (*write_u8_to_domain_function)(u32, u8));

  static int do_write_unsigned_int(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32));
  static int do_write_signed_int(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32));

  static int do_write_float(lua_State* luaState, void (*write_u32_to_domain_function)(u32, u32));
  static int do_write_double(lua_State* luaState, void (*write_u64_to_domain_function)(u32, u64));

  static int do_write_fixed_length_string(lua_State* luaState, void (*copy_string_to_domain_function)(u32, const char*, size_t));
  static int do_write_null_terminated_string(lua_State* luaState, void (*copy_string_to_domain_function)(u32, const char*, size_t));
};

}  // namespace Lua
