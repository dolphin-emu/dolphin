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
  }
}
