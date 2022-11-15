#pragma once
#include <string>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"
#include "Core/HW/Memmap.h"

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

    int read_unsigned_8(lua_State* luaState);
     int read_unsigned_16(lua_State* luaState);
     int read_unsigned_32(lua_State* luaState);
     int read_unsigned_64(lua_State* luaState);

     int read_signed_8(lua_State* luaState);
     int read_signed_16(lua_State* luaState);
     int read_signed_32(lua_State* luaState);
     int read_signed_64(lua_State* luaState);

     int read_float(lua_State* luaState);
     int read_double(lua_State* luaState);

     int read_unsigned_byte(lua_State* luaState);
     int read_signed_byte(lua_State* luaState);

     int read_unsigned_int(lua_State* luaState);
     int read_signed_int(lua_State* luaState);

     int read_fixed_length_string(lua_State* luaState);
     int read_null_terminated_string(lua_State* luaState);

     int write_unsigned_8(lua_State* luaState);
     int write_unsigned_16(lua_State* luaState);
     int write_unsigned_32(lua_State* luaState);
     int write_unsigned_64(lua_State* luaState);

     int write_signed_8(lua_State* luaState);
     int write_signed_16(lua_State* luaState);
     int write_signed_32(lua_State* luaState);
     int write_signed_64(lua_State* luaState);

     int write_float(lua_State* luaState);
     int write_double(lua_State* luaState);

     int write_unsigned_byte(lua_State* luaState);
     int write_signed_byte(lua_State* luaState);

     int write_unsigned_int(lua_State* luaState);
     int write_signed_int(lua_State* luaState);

     int write_fixed_length_string(lua_State* luaState);
     int write_null_terminated_string(lua_State* luaState);

     u8 read_u8_from_domain(u32 address, lua_State* luaState);
     u16 read_u16_from_domain(u32 address, lua_State* luaState);
     u32 read_u32_from_domain(u32 address, lua_State* luaState);
     u64 read_u64_from_domain(u32 address, lua_State* luaState);
     std::string get_string_from_domain(u32 address, u32 length, lua_State* luaState);
     u8* get_pointer_to_domain(u32 address, lua_State* luaState);

     void write_u8_to_domain(u32 address, u8 val, lua_State* luaState);
     void write_u16_to_domain(u32 address, u16 val, lua_State* luaState);
     void write_u32_to_domain(u32 address, u32 val, lua_State* luaState);
     void write_u64_to_domain(u32 address, u64 val, lua_State* luaState);
     void copy_string_to_domain(u32 address, const char* stringToWrite, size_t numBytes, lua_State* luaState);
  }
}
