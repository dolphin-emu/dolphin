#include "LuaRamFunctions.h"
#include <format>
#include "Core/HW/Memmap.h"
namespace Lua
{
namespace Lua_RAM
{

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

  luaL_Reg* luaRamFunctions = new luaL_Reg[]{

      {"read_unsigned_8", ram_read_unsigned_8},
      {"read_unsigned_16", ram_read_unsigned_16},
      {"read_unsigned_32", ram_read_unsigned_32},
      {"read_unsigned_64", ram_read_unsigned_64},
      {"read_signed_8", ram_read_signed_8},
      {"read_signed_16", ram_read_signed_16},
      {"read_signed_32", ram_read_signed_32},
      {"read_signed_64", ram_read_signed_64},
      {"read_float", ram_read_float},
      {"read_double", ram_read_double},
      {"read_unsigned_byte", ram_read_unsigned_byte},
      {"read_signed_byte", ram_read_signed_byte},
      {"read_unsigned_int", ram_read_unsigned_int},
      {"read_signed_int", ram_read_signed_int},
      {"read_fixed_length_string", ram_read_fixed_length_string},
      {"read_null_terminated_string", ram_read_null_terminated_string},

      {"write_unsigned_8", ram_write_unsigned_8},
      {"write_unsigned_16", ram_write_unsigned_16},
      {"write_unsigned_32", ram_write_unsigned_32},
      {"write_unsigned_64", ram_write_unsigned_64},
      {"write_signed_8", ram_write_signed_8},
      {"write_signed_16", ram_write_signed_16},
      {"write_signed_32", ram_write_signed_32},
      {"write_signed_64", ram_write_signed_64},
      {"write_float", ram_write_float},
      {"write_double", ram_write_double},
      {"write_unsigned_byte", ram_write_unsigned_byte},
      {"write_signed_byte", ram_write_signed_byte},
      {"write_unsigned_int", ram_write_unsigned_int},
      {"write_signed_int", ram_write_signed_int},
      {"write_fixed_length_string", ram_write_fixed_length_string},
      {"write_null_terminated_string", ram_write_null_terminated_string},

      {nullptr, nullptr}
  };

  luaL_setfuncs(luaState, luaRamFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "ram");
}


int ram_read_unsigned_8(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_unsigned_8(luaState, ram_read_u8_from_domain_helper_function);
}

int ram_read_unsigned_16(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_unsigned_16(luaState, ram_read_u16_from_domain_helper_function);
}

int ram_read_unsigned_32(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_unsigned_32(luaState, ram_read_u32_from_domain_helper_function);
}

int ram_read_unsigned_64(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_unsigned_64(luaState, ram_read_u64_from_domain_helper_function);
}

int ram_read_signed_8(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_signed_8(luaState, ram_read_u8_from_domain_helper_function);
}

int ram_read_signed_16(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_signed_16(luaState, ram_read_u16_from_domain_helper_function);
}

int ram_read_signed_32(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_signed_32(luaState, ram_read_u32_from_domain_helper_function);
}

int ram_read_signed_64(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_signed_64(luaState, ram_read_u64_from_domain_helper_function);
}

int ram_read_float(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_float(luaState, ram_read_u32_from_domain_helper_function);
}

int ram_read_double(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_double(luaState, ram_read_u64_from_domain_helper_function);
}

int ram_read_unsigned_byte(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_unsigned_byte(luaState, ram_read_u8_from_domain_helper_function);
}

int ram_read_signed_byte(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_signed_byte(luaState, ram_read_u8_from_domain_helper_function);
}

int ram_read_unsigned_int(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_unsigned_int(luaState, ram_read_u32_from_domain_helper_function);
}

int ram_read_signed_int(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_signed_int(luaState, ram_read_u32_from_domain_helper_function);
}

int ram_read_fixed_length_string(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_fixed_length_string(luaState, ram_get_string_from_domain_helper_function);
}

int ram_read_null_terminated_string(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_read_null_terminated_string(luaState, ram_get_pointer_to_domain_helper_function, ram_get_string_from_domain_helper_function);
}

int ram_write_unsigned_8(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_unsigned_8(luaState, ram_write_u8_to_domain_helper_function);
}

int ram_write_unsigned_16(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_unsigned_16(luaState, ram_write_u16_to_domain_helper_function);
}

int ram_write_unsigned_32(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_unsigned_32(luaState, ram_write_u32_to_domain_helper_function);
}

int ram_write_unsigned_64(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_unsigned_64(luaState, ram_write_u64_to_domain_helper_function);
}

int ram_write_signed_8(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_signed_8(luaState, ram_write_u8_to_domain_helper_function);
}

int ram_write_signed_16(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_signed_16(luaState, ram_write_u16_to_domain_helper_function);
}

int ram_write_signed_32(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_signed_32(luaState, ram_write_u32_to_domain_helper_function);
}

int ram_write_signed_64(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_signed_64(luaState, ram_write_u64_to_domain_helper_function);
}

int ram_write_float(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_float(luaState, ram_write_u32_to_domain_helper_function);
}

int ram_write_double(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_double(luaState, ram_write_u64_to_domain_helper_function);
}

int ram_write_unsigned_byte(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_unsigned_byte(luaState, ram_write_u8_to_domain_helper_function);
}

int ram_write_signed_byte(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_signed_byte(luaState, ram_write_u8_to_domain_helper_function);
}

int ram_write_unsigned_int(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_unsigned_int(luaState, ram_write_u32_to_domain_helper_function);
}

int ram_write_signed_int(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_signed_int(luaState, ram_write_u32_to_domain_helper_function);
}

int ram_write_fixed_length_string(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_fixed_length_string(luaState, ram_copy_string_to_domain_helper_function);
}

int ram_write_null_terminated_string(lua_State* luaState)
{
  return LuaMemoryBaseApi::do_write_null_terminated_string(luaState, ram_copy_string_to_domain_helper_function);
}


u8 ram_read_u8_from_domain_helper_function(u32 address)
{
  return Memory::Read_U8(address);
}

u16 ram_read_u16_from_domain_helper_function(u32 address)
{
  return Memory::Read_U16(address);
}

u32 ram_read_u32_from_domain_helper_function(u32 address)
{
  return Memory::Read_U32(address);
}

u64 ram_read_u64_from_domain_helper_function(u32 address)
{
  return Memory::Read_U64(address);
}

std::string ram_get_string_from_domain_helper_function(u32 address, u32 length)
{
  return Memory::GetString(address, length);
}

u8* ram_get_pointer_to_domain_helper_function(u32 address)
{
  return Memory::GetPointer(address);
}

void ram_write_u8_to_domain_helper_function(u32 address, u8 val)
{
  Memory::Write_U8(val, address);
}

void ram_write_u16_to_domain_helper_function(u32 address, u16 val)
{
  Memory::Write_U16(val, address);
}

void ram_write_u32_to_domain_helper_function(u32 address, u32 val)
{
  Memory::Write_U32(val, address);
}

void ram_write_u64_to_domain_helper_function(u32 address, u64 val)
{
  Memory::Write_U64(val, address);
}

void ram_copy_string_to_domain_helper_function(u32 address, const char* stringToWrite, size_t numBytes)
{
  Memory::CopyToEmu(address, stringToWrite, numBytes);
}
}
}
