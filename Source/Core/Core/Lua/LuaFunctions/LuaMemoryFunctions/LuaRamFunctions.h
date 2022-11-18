#pragma once
#include <string>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "Core/HW/Memmap.h"
#include "LuaMemoryBaseApi.h"

namespace Lua
{
class RAM
{
public:
  RAM() {}
};

class LuaRamFunctions : LuaMemoryBaseApi<RAM>
{
public:

  static void InitLuaRamFunctions(lua_State* luaState) {
    read_u8_from_domain_function = ram_read_u8_from_domain_helper_function;
    read_u16_from_domain_function = ram_read_u16_from_domain_helper_function;
    read_u32_from_domain_function = ram_read_u32_from_domain_helper_function;
    read_u64_from_domain_function = ram_read_u64_from_domain_helper_function;
    get_string_from_domain_function = ram_get_string_from_domain_helper_function;
    get_pointer_to_domain_function = ram_get_pointer_to_domain_helper_function;
    write_u8_to_domain_function = ram_write_u8_to_domain_helper_function;
    write_u16_to_domain_function = ram_write_u16_to_domain_helper_function;
    write_u32_to_domain_function = ram_write_u32_to_domain_helper_function;
    write_u64_to_domain_function = ram_write_u64_to_domain_helper_function;
    copy_string_to_domain_function = ram_copy_string_to_domain_helper_function;

    luaL_Reg* luaRamFunctions = new luaL_Reg[]{

        {"read_unsigned_8", do_read_unsigned_8},
        {"read_unsigned_16", do_read_unsigned_16},
        {"read_unsigned_32", do_read_unsigned_32},
        {"read_unsigned_64", do_read_unsigned_64},
        {"read_signed_8", do_read_signed_8},
        {"read_signed_16", do_read_signed_16},
        {"read_signed_32", do_read_signed_32},
        {"read_signed_64", do_read_signed_64},
        {"read_float", do_read_float},
        {"read_double", do_read_double},
        {"read_unsigned_byte", do_read_unsigned_byte},
        {"read_signed_byte", do_read_signed_byte},
        {"read_unsigned_int", do_read_unsigned_int},
        {"read_signed_int", do_read_signed_int},
        {"read_fixed_length_string", do_read_fixed_length_string},
        {"read_null_terminated_string", do_read_null_terminated_string},

        {"write_unsigned_8", do_write_unsigned_8},
        {"write_unsigned_16", do_write_unsigned_16},
        {"write_unsigned_32", do_write_unsigned_32},
        {"write_unsigned_64", do_write_unsigned_64},
        {"write_signed_8", do_write_signed_8},
        {"write_signed_16", do_write_signed_16},
        {"write_signed_32", do_write_signed_32},
        {"write_signed_64", do_write_signed_64},
        {"write_float", do_write_float},
        {"write_double", do_write_double},
        {"write_unsigned_byte", do_write_unsigned_byte},
        {"write_signed_byte", do_write_signed_byte},
        {"write_unsigned_int", do_write_unsigned_int},
        {"write_signed_int", do_write_signed_int},
        {"write_fixed_length_string", do_write_fixed_length_string},
        {"write_null_terminated_string", do_write_null_terminated_string},

        {nullptr, nullptr}};

    LuaMemoryBaseInit(luaState, "ram", luaRamFunctions);
  }

static u8 ram_read_u8_from_domain_helper_function(u32 address) { return Memory::Read_U8(address); }
static u16 ram_read_u16_from_domain_helper_function(u32 address) { return Memory::Read_U16(address); }
static u32 ram_read_u32_from_domain_helper_function(u32 address) { return Memory::Read_U32(address); }
static u64 ram_read_u64_from_domain_helper_function(u32 address) { return Memory::Read_U64(address); }
static std::string ram_get_string_from_domain_helper_function(u32 address, u32 length) { return Memory::GetString(address, length); }
static u8* ram_get_pointer_to_domain_helper_function(u32 address) { return Memory::GetPointer(address); }
static void ram_write_u8_to_domain_helper_function(u32 address, u8 val) { Memory::Write_U8(val, address); }
static void ram_write_u16_to_domain_helper_function(u32 address, u16 val) { Memory::Write_U16(val, address); }
static void ram_write_u32_to_domain_helper_function(u32 address, u32 val) { Memory::Write_U32(val, address); }
static void ram_write_u64_to_domain_helper_function(u32 address, u64 val) { Memory::Write_U64(val, address); }
static void ram_copy_string_to_domain_helper_function(u32 address, const char* stringToWrite, size_t numBytes) { Memory::CopyToEmu(address, stringToWrite, numBytes); }
};
}
