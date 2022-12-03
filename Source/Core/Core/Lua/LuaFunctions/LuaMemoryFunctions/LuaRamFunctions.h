#pragma once
#include <string>

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

#include "Core/HW/Memmap.h"
#include "LuaMemoryBaseApi.h"
#include "../LuaByteWrapper.h"

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

      {"readFrom", do_read_into_byte_wrapper},
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
        {"read_unsigned_bytes", do_read_unsigned_bytes},
        {"read_signed_bytes", do_read_signed_bytes},

        {"writeTo", do_write_from_byte_wrapper},
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

  static int do_read_into_byte_wrapper(lua_State* luaState)
  {
    ByteWrapper* byteWrapperReturnResult = NULL;
    u32 address = luaL_checkinteger(luaState, 2);
    std::optional<PowerPC::ReadResult<u64>> readResultSize64 = PowerPC::HostTryReadU64(address);
    if (readResultSize64.has_value())
    {
      u64 u64Result = readResultSize64.value().value;
      byteWrapperReturnResult = ByteWrapper::CreateByteWrapperFromU64(u64Result);
    }
    else
    {
      std::optional<PowerPC::ReadResult<u32>> readResultSize32 = PowerPC::HostTryReadU32(address);
      if (readResultSize32.has_value())
      {
        u32 u32Result = readResultSize32.value().value;
        byteWrapperReturnResult = ByteWrapper::CreateByteWrapperFromU32(u32Result);
      }
      else
      {
        std::optional<PowerPC::ReadResult<u16>> readResultSize16 = PowerPC::HostTryReadU16(address);
        if (readResultSize16.has_value())
        {
          u16 u16Result = readResultSize16.value().value;
          byteWrapperReturnResult = ByteWrapper::CreateByteWrapperFromU16(u16Result);
        }
        else
        {
          std::optional<PowerPC::ReadResult<u8>> readResultSize8 = PowerPC::HostTryReadU8(address);
          if (readResultSize8.has_value())
          {
            u8 u8Result = readResultSize8.value().value;
            byteWrapperReturnResult = ByteWrapper::CreateByteWrapperFromU8(u8Result);
          }
          else
          {
            luaL_error(luaState, "Error: Could not read result in readFrom() function.");
            return 0;
          }
        }
      }
    }

    *reinterpret_cast<ByteWrapper**>(lua_newuserdata(luaState, sizeof(ByteWrapper*))) = byteWrapperReturnResult;
    luaL_setmetatable(luaState, LuaByteWrapper::LUA_BYTE_WRAPPER);
    return 1;
    
  }

  static int do_write_from_byte_wrapper(lua_State* luaState)
  {
    u32 address = luaL_checkinteger(luaState, 2);

    ByteWrapper::ByteType byteType = ByteWrapper::ByteType::UNDEFINED;
    ByteWrapper* byteWrapperPointer =
        (*reinterpret_cast<ByteWrapper**>(luaL_checkudata(luaState, 3, LuaByteWrapper::LUA_BYTE_WRAPPER)));
    if (lua_gettop(luaState) < 4)
    {
      byteType = byteWrapperPointer->byteType;
    }
    else
    {
      const char* inputTypeString = luaL_checkstring(luaState, 4);
      byteType = ByteWrapper::parseType(inputTypeString);
    }

    if (!ByteWrapper::typeSizeCheck(luaState, byteWrapperPointer, byteType, "Error: invalid/undefined type in writeTo method for ByteWrapper"))
    {
      luaL_error(luaState, "Error: ByteWrapper was not big enough to write the specified type to memory!");
      return 0;
    }
    u8 u8Val = 0;
    u16 u16Val = 0;
    u32 u32Val = 0;
    u64 u64Val = 0ULL;

    switch (byteType)
    {

    case ByteWrapper::ByteType::UNSIGNED_8:
    case ByteWrapper::ByteType::SIGNED_8:
      u8Val = byteWrapperPointer->getValueAsU8();
      ram_write_u8_to_domain_helper_function(luaState, address, u8Val);
      return 0;

    case ByteWrapper::ByteType::UNSIGNED_16:
    case ByteWrapper::ByteType::SIGNED_16:
      u16Val = byteWrapperPointer->getValueAsU16();
      ram_write_u16_to_domain_helper_function(luaState, address, u16Val);
      return 0;

    case ByteWrapper::ByteType::UNSIGNED_32:
    case ByteWrapper::ByteType::SIGNED_32:
    case ByteWrapper::ByteType::FLOAT:
      u32Val = byteWrapperPointer->getValueAsU32();
      ram_write_u32_to_domain_helper_function(luaState, address, u32Val);
      return 0;

    case ByteWrapper::ByteType::UNSIGNED_64:
    case ByteWrapper::ByteType::SIGNED_64:
    case ByteWrapper::ByteType::DOUBLE:
      u64Val = byteWrapperPointer->getValueAsU64();
      ram_write_u64_to_domain_helper_function(luaState, address, u64Val);
      return 0;

    default:
      luaL_error(luaState, "Error: Type was not specified for ByteWrapper for WriteTo() method.");
      return 0;
    }
  }

  static u8 ram_read_u8_from_domain_helper_function(lua_State* luaState, u32 address)
  {
  std::optional<PowerPC::ReadResult<u8>> readResult = PowerPC::HostTryReadU8(address);
  if (!readResult.has_value())
  {
      luaL_error(luaState, "Error: Attempt to read_u8 from memory failed!");
      return 1;
    }
  return readResult.value().value;
}

static u16 ram_read_u16_from_domain_helper_function(lua_State* luaState, u32 address)
{
  std::optional<PowerPC::ReadResult<u16>> readResult = PowerPC::HostTryReadU16(address);
  if (!readResult.has_value())
  {
    luaL_error(luaState, "Error: Attempt to read_u16 from memory failed!");
    return 1;
  }
  return readResult.value().value;
}

static u32 ram_read_u32_from_domain_helper_function(lua_State* luaState, u32 address)
{
  std::optional<PowerPC::ReadResult<u32>> readResult = PowerPC::HostTryReadU32(address);
  if (!readResult.has_value())
  {
    luaL_error(luaState, "Error: Attempt to read_u32 from memory failed!");
    return 1;
  }
  return readResult.value().value;
}

static u64 ram_read_u64_from_domain_helper_function(lua_State* luaState, u32 address)
{
  std::optional<PowerPC::ReadResult<u64>> readResult = PowerPC::HostTryReadU64(address);
  if (!readResult.has_value())
  {
    luaL_error(luaState, "Error: Attempt to read_u64 from memory failed!");
    return 1;
  }
  return readResult.value().value;
}

static std::string ram_get_string_from_domain_helper_function(lua_State* luaState, u32 address, u32 length)
{
  std::optional<PowerPC::ReadResult<std::string>> readResult = PowerPC::HostTryReadString(address, length);
  if (!readResult.has_value())
  {
    luaL_error(luaState, "Error: Attempt to read string from memory failed!");
    return "";
  }
  return readResult.value().value;
}

static u8* ram_get_pointer_to_domain_helper_function(lua_State* luaState, u32 address)
{
  return Memory::GetPointer(address);
}

static void ram_write_u8_to_domain_helper_function(lua_State* luaState, u32 address, u8 val)
{
  std::optional<PowerPC::WriteResult> writeResult = PowerPC::HostTryWriteU8(val, address);
  if (!writeResult.has_value())
    luaL_error(luaState, "Error: Attempt to write_u8 to memory failed!");
}

static void ram_write_u16_to_domain_helper_function(lua_State* luaState, u32 address, u16 val)
{
  std::optional<PowerPC::WriteResult> writeResult = PowerPC::HostTryWriteU16(val, address);
  if (!writeResult.has_value())
    luaL_error(luaState, "Error: Attempt to write_u16 to memory failed!");
}

static void ram_write_u32_to_domain_helper_function(lua_State* luaState, u32 address, u32 val)
{
  std::optional<PowerPC::WriteResult> writeResult = PowerPC::HostTryWriteU32(val, address);
  if (!writeResult.has_value())
    luaL_error(luaState, "Error: Attempt to write_u32 to memory failed!");
}

static void ram_write_u64_to_domain_helper_function(lua_State* luaState, u32 address, u64 val)
{
  std::optional<PowerPC::WriteResult> writeResult = PowerPC::HostTryWriteU64(val, address);
  if (!writeResult.has_value())
    luaL_error(luaState, "Error: Attempt to write_u64 to memory failed!");
}

static void ram_copy_string_to_domain_helper_function(lua_State* luaState, u32 address, const char* stringToWrite, size_t numBytes)
{
  Memory::CopyToEmu(address, stringToWrite, numBytes);
}
};
}
