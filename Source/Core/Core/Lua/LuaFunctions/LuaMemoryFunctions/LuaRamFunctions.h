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

  namespace Lua_RAM
  {

    class RAM
    {
    public:
      inline RAM() {}
    };

    extern RAM* ramPointer;

    RAM* GetRamInstance();
    void InitLuaRAMFunctions(lua_State* luaState);

    int ram_read_unsigned_8(lua_State* luaState);
    int ram_read_unsigned_16(lua_State* luaState);
    int ram_read_unsigned_32(lua_State* luaState);
    int ram_read_unsigned_64(lua_State* luaState);

    int ram_read_signed_8(lua_State* luaState);
    int ram_read_signed_16(lua_State* luaState);
    int ram_read_signed_32(lua_State* luaState);
    int ram_read_signed_64(lua_State* luaState);

    int ram_read_float(lua_State* luaState);
    int ram_read_double(lua_State* luaState);

    int ram_read_unsigned_byte(lua_State* luaState);
    int ram_read_signed_byte(lua_State* luaState);

    int ram_read_unsigned_int(lua_State* luaState);
    int ram_read_signed_int(lua_State* luaState);

    int ram_read_fixed_length_string(lua_State* luaState);
    int ram_read_null_terminated_string(lua_State* luaState);

    int ram_write_unsigned_8(lua_State* luaState);
    int ram_write_unsigned_16(lua_State* luaState);
    int ram_write_unsigned_32(lua_State* luaState);
    int ram_write_unsigned_64(lua_State* luaState);

    int ram_write_signed_8(lua_State* luaState);
    int ram_write_signed_16(lua_State* luaState);
    int ram_write_signed_32(lua_State* luaState);
    int ram_write_signed_64(lua_State* luaState);

    int ram_write_float(lua_State* luaState);
    int ram_write_double(lua_State* luaState);

    int ram_write_unsigned_byte(lua_State* luaState);
    int ram_write_signed_byte(lua_State* luaState);

    int ram_write_unsigned_int(lua_State* luaState);
    int ram_write_signed_int(lua_State* luaState);

    int ram_write_fixed_length_string(lua_State* luaState);
    int ram_write_null_terminated_string(lua_State* luaState);

    u8 ram_read_u8_from_domain_helper_function(u32 address);
    u16 ram_read_u16_from_domain_helper_function(u32 address);
    u32 ram_read_u32_from_domain_helper_function(u32 address);
    u64 ram_read_u64_from_domain_helper_function(u32 address);
    std::string ram_get_string_from_domain_helper_function(u32 address, u32 length);
    u8* ram_get_pointer_to_domain_helper_function(u32 address);

    void ram_write_u8_to_domain_helper_function(u32 address, u8 val);
    void ram_write_u16_to_domain_helper_function(u32 address, u16 val);
    void ram_write_u32_to_domain_helper_function(u32 address, u32 val);
    void ram_write_u64_to_domain_helper_function(u32 address, u64 val);
    void ram_copy_string_to_domain_helper_function(u32 address, const char* stringToWrite, size_t numBytes);
  }
}
