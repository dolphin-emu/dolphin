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

  luaL_Reg* luaRamFunctions = new luaL_Reg[]{

      {"read_unsigned_8", read_unsigned_8},
      {"read_unsigned_16", read_unsigned_16},
      {"read_unsigned_32", read_unsigned_32},
      {"read_unsigned_64", read_unsigned_64},
      {"read_signed_8", read_signed_8},
      {"read_signed_16", read_signed_16},
      {"read_signed_32", read_signed_32},
      {"read_signed_64", read_signed_64},
      {"read_float", read_float},
      {"read_double", read_double},
      {"read_unsigned_byte", read_unsigned_byte},
      {"read_signed_byte", read_signed_byte},
      {"read_unsigned_int", read_unsigned_int},
      {"read_signed_int", read_signed_int},
      {"read_fixed_length_string", read_fixed_length_string},
      {"read_null_terminated_string", read_null_terminated_string},

      {"write_unsigned_8", write_unsigned_8},
      {"write_unsigned_16", write_unsigned_16},
      {"write_unsigned_32", write_unsigned_32},
      {"write_unsigned_64", write_unsigned_64},
      {"write_signed_8", write_signed_8},
      {"write_signed_16", write_signed_16},
      {"write_signed_32", write_signed_32},
      {"write_signed_64", write_signed_64},
      {"write_float", write_float},
      {"write_double", write_double},
      {"write_unsigned_byte", write_unsigned_byte},
      {"write_signed_byte", write_signed_byte},
      {"write_unsigned_int", write_unsigned_int},
      {"write_signed_int", write_signed_int},
      {"write_fixed_length_string", write_fixed_length_string},
      {"write_null_terminated_string", write_null_terminated_string},

      {nullptr, nullptr}
  };

  luaL_setfuncs(luaState, luaRamFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "ram");

}

    
int read_unsigned_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushnumber(luaState, read_u8_from_domain(address, luaState));
  return 1;
}


int read_unsigned_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushnumber(luaState, read_u16_from_domain(address, luaState));
  return 1;
}


int read_unsigned_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushnumber(luaState, read_u32_from_domain(address, luaState));
  return 1;
}


int read_unsigned_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushnumber(luaState, read_u64_from_domain(address, luaState));
  return 1;
}


int read_signed_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u8 rawVal = read_u8_from_domain(address, luaState);
  s8 signedByte = 0;
  memcpy(&signedByte, &rawVal, sizeof(s8));
  lua_pushnumber(luaState, signedByte);
  return 1;
}


int read_signed_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u16 rawVal = read_u16_from_domain(address, luaState);
  s16 signedShort = 0;
  memcpy(&signedShort, &rawVal, sizeof(s16));
  lua_pushnumber(luaState, signedShort);
  return 1;
}


int read_signed_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u32 rawVal = read_u32_from_domain(address, luaState);
  s32 signedInt = 0;
  memcpy(&signedInt, &rawVal, sizeof(s32));
  lua_pushnumber(luaState, signedInt);
  return 1;
}


int read_signed_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u64 rawVal = read_u64_from_domain(address, luaState);
  s64 signedLongLong = 0;
  memcpy(&signedLongLong, &rawVal, sizeof(s64));
  lua_pushnumber(luaState, signedLongLong);
  return 1;
}


int read_float(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u32 rawVal = read_u32_from_domain(address, luaState);
  float myFloat = 0.0;
  memcpy(&myFloat, &rawVal, sizeof(float));
  lua_pushnumber(luaState, myFloat);
  return 1;
}


int read_double(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u64 rawVal = read_u64_from_domain(address, luaState);
  double myDouble = 0.0;
  memcpy(&myDouble, &rawVal, sizeof(double));
  lua_pushnumber(luaState, myDouble);
  return 1;
}


int read_unsigned_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushnumber(luaState, read_u8_from_domain(address, luaState));
  return 1;
}


int read_signed_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u8 rawVal = read_u8_from_domain(address, luaState);
  s8 signedByte = 0;
  memcpy(&signedByte, &rawVal, sizeof(s8));
  lua_pushnumber(luaState, signedByte);
  return 1;
}


int read_unsigned_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  lua_pushnumber(luaState, read_u32_from_domain(address, luaState));
  return 1;
}


int read_signed_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u32 rawVal = read_u32_from_domain(address, luaState);
  s32 signedInt = 0;
  memcpy(&signedInt, &rawVal, sizeof(s32));
  lua_pushnumber(luaState, signedInt);
  return 1;
}


int read_fixed_length_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u32 stringLength = luaL_checkinteger(luaState, 2);
  std::string returnResult = get_string_from_domain(address, stringLength, luaState);
  lua_pushfstring(luaState, returnResult.c_str());
  return 1;
}


int read_null_terminated_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u8* memPointerStart = get_pointer_to_domain(address, luaState);
  u8* memPointerCurrent = memPointerStart;

  while (*memPointerCurrent != '\0')
    memPointerCurrent++;

  std::string returnResult =
      get_string_from_domain(address, (u32)(memPointerCurrent - memPointerStart), luaState);
  lua_pushfstring(luaState, returnResult.c_str());
  return 1;
}


int write_unsigned_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u8 writeVal = luaL_checkinteger(luaState, 2);
  write_u8_to_domain(address, writeVal, luaState);
  return 0;
}


int write_unsigned_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u16 writeVal = luaL_checkinteger(luaState, 2);
  write_u16_to_domain(address, writeVal, luaState);
  return 0;
}


int write_unsigned_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u32 writeVal = luaL_checkinteger(luaState, 2);
  write_u32_to_domain(address, writeVal, luaState);
  return 0;
}


int write_unsigned_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u64 writeVal = luaL_checknumber(luaState, 2);
  write_u64_to_domain(address, writeVal, luaState);
  return 0;
}


int write_signed_8(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  s8 rawVal = luaL_checkinteger(luaState, 2);
  u8 convertedByte = 0;
  memcpy(&convertedByte, &rawVal, sizeof(u8));
  write_u8_to_domain(address, convertedByte, luaState);
  return 0;
}


int write_signed_16(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  s16 rawVal = luaL_checkinteger(luaState, 2);
  u16 convertedShort = 0;
  memcpy(&convertedShort, &rawVal, sizeof(u16));
  write_u16_to_domain(address, convertedShort, luaState);
  return 0;
}


int write_signed_32(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  s32 rawVal = luaL_checkinteger(luaState, 2);
  u32 convertedInt = 0;
  memcpy(&convertedInt, &rawVal, sizeof(u32));
  write_u32_to_domain(address, convertedInt, luaState);
  return 0;
}


int write_signed_64(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  s64 rawVal = luaL_checknumber(luaState, 2);
  u64 convertedLongLong = 0;
  memcpy(&convertedLongLong, &rawVal, sizeof(u64));
  write_u64_to_domain(address, convertedLongLong, luaState);
  return 0;
}


int write_float(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  float rawVal = luaL_checknumber(luaState, 2);
  u32 convertedInt = 0;
  memcpy(&convertedInt, &rawVal, sizeof(u32));
  write_u32_to_domain(address, convertedInt, luaState);
  return 0;
}


int write_double(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  double rawVal = luaL_checknumber(luaState, 2);
  u64 convertedLongLong = 0;
  memcpy(&convertedLongLong, &rawVal, sizeof(u64));
  write_u64_to_domain(address, convertedLongLong, luaState);
  return 0;
}


int write_unsigned_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u8 rawVal = luaL_checkinteger(luaState, 2);
  write_u8_to_domain(address, rawVal, luaState);
  return 0;
}


int write_signed_byte(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  s8 rawVal = luaL_checkinteger(luaState, 2);
  u8 convertedByte = 0;
  memcpy(&convertedByte, &rawVal, sizeof(u8));
  write_u8_to_domain(address, convertedByte, luaState);
  return 0;
}


int write_unsigned_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  u32 rawVal = luaL_checkinteger(luaState, 2);
  write_u32_to_domain(address, rawVal, luaState);
  return 0;
}


int write_signed_int(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  s32 rawVal = luaL_checkinteger(luaState, 2);
  u32 convertedInt = 0;
  memcpy(&convertedInt, &rawVal, sizeof(u32));
  write_u32_to_domain(address, convertedInt, luaState);
  return 0;
}


int write_fixed_length_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  const char* stringToWrite = luaL_checkstring(luaState, 2);
  u32 stringSize = luaL_checkinteger(luaState, 3);
  copy_string_to_domain(address, stringToWrite, stringSize, luaState);
  return 0;
}


int write_null_terminated_string(lua_State* luaState)
{
  u32 address = luaL_checkinteger(luaState, 1);
  const char* stringToWrite = luaL_checkstring(luaState, 2);
  size_t stringSize = std::strlen(stringToWrite);
  copy_string_to_domain(address, stringToWrite, stringSize, luaState);
  return 0;
}


u8 read_u8_from_domain(u32 address, lua_State* luaState)
{
  return Memory::Read_U8(address);
}


u16 read_u16_from_domain(u32 address, lua_State* luaState)
{
  return Memory::Read_U16(address);
}


u32 read_u32_from_domain(u32 address, lua_State* luaState)
{
  return Memory::Read_U32(address);
}


u64 read_u64_from_domain(u32 address, lua_State* luaState)
{
  return Memory::Read_U64(address);
}


std::string get_string_from_domain(u32 address, u32 length, lua_State* luaState)
{
  return Memory::GetString(address, length);
}

u8* get_pointer_to_domain(u32 address, lua_State* luaState)
{
  return Memory::GetPointer(address);
}

void write_u8_to_domain(u32 address, u8 val, lua_State* luaState)
{
  Memory::Write_U8(val, address);
}


void write_u16_to_domain(u32 address, u16 val, lua_State* luaState)
{
  Memory::Write_U16(val, address);
}


void write_u32_to_domain(u32 address, u32 val, lua_State* luaState)
{
  Memory::Write_U32(val, address);
}


void write_u64_to_domain(u32 address, u64 val, lua_State* luaState)
{
  Memory::Write_U64(val, address);
}


void copy_string_to_domain(u32 address, const char* stringToWrite, size_t numBytes, lua_State* luaState)
{
  Memory::CopyToEmu(address, stringToWrite, numBytes);
}
}
}
