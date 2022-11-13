#include "LuaRamFunctions.h"
#include <format>
#include "Core/HW/Memmap.h"
namespace Lua
{
namespace Lua_RAM
{

void CheckIfRamAddressOutOfBoundsErrorFunc(lua_State* luaState, const std::string& funcName, u32 address)
{
  if (address >= Memory::GetRamSizeReal())
  {
    luaL_error(luaState, (std::string("Address Out of Bounds Error in function ram.") + funcName + ": Address of " + std::format("{:#08x}", address) + " exceeds maximum valid RAM address of " + std::format("{:#08x}", Memory::GetRamSizeReal() - 1)).c_str());
  }
}

RAM* ramPointer = NULL;

RAM* GetRamInstance()
{
  if (ramPointer == NULL)
  {
    ramPointer = new RAM();
  }

  return ramPointer;
}

void InitLuaRAMFunctions(lua_State* luaState)
{
  RAM** ramPtrPtr = (RAM**)lua_newuserdata(luaState, sizeof(RAM*));
  *ramPtrPtr = GetRamInstance();
  luaL_newmetatable(luaState, "LuaRamMetaTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaRamFunctions[] = {
    {"read_unsigned_8", ram_read_unsigned_8},
    {"read_unsigned_16", ram_read_unsigned_16},
    {"read_unsigned_32", ram_read_unsigned_32},
    {"read_unsigned_64", ram_read_unsigned_64},
    {"ram_read_signed_8", ram_read_signed_8},
    {"ram_read_signed_16", ram_read_signed_16},
    {"ram_read_signed_32", ram_read_signed_32},
    {"ram_read_signed_64", ram_read_signed_64},
    {nullptr, nullptr}
  };

  luaL_setfuncs(luaState, luaRamFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "ram");

}

    int ram_read_unsigned_8(lua_State* luaState)
    {
      //(RAM**)luaL_checkudata(luaState, 1, "LuaRamMetaTable");
      u32 address = luaL_checkinteger(luaState, 1);
      CheckIfRamAddressOutOfBoundsErrorFunc(luaState, "read_unsigned_8", address);
      
      lua_pushnumber(luaState, Memory::Read_U8(address));
      return 1;
    }


    int ram_read_unsigned_16(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_unsigned_32(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_unsigned_64(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_signed_8(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_signed_16(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_signed_32(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_signed_64(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_float(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_double(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_unsigned_byte(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_signed_byte(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_unsigned_int(lua_State* luaState)
    {
      return 1;
    }

    int ram_read_signed_int(lua_State* luaState)
    {
      return 1;
    }

    int ram_write_unsigned_8(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_unsigned_16(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_unsigned_32(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_unsigned_64(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_signed_8(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_signed_16(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_signed_32(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_signed_64(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_float(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_double(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_unsigned_byte(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_signed_byte(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_unsigned_int(lua_State* luaState)
    {
      return 0;
    }

    int ram_write_signed_int(lua_State* luaState)
    {
      return 0;
    }

  }
} 
