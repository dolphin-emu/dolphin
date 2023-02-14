#include "Core/Lua/LuaFunctions/LuaMemoryApi.h"

#include <fmt/format.h>
#include <optional>

#include "Core/HW/Memmap.h"
#include "Core/Lua/LuaHelperClasses/NumberType.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"

namespace Lua::LuaMemoryApi
{

class MEMORY
{
public:
  inline MEMORY() {}
};

static std::unique_ptr<MEMORY> instance = nullptr;
static const char* class_name = "memoryAPI";

MEMORY* GetInstance()
{
  if (instance == nullptr)
    instance = std::make_unique<MEMORY>(MEMORY());
  return instance.get();
}

void InitLuaMemoryApi(lua_State* lua_state, const std::string& lua_api_version)
{
  MEMORY** memory_instance_ptr_ptr = (MEMORY**)lua_newuserdata(lua_state, sizeof(MEMORY*));
  *memory_instance_ptr_ptr = GetInstance();
  luaL_newmetatable(lua_state, "LuaMemoryFunctionsMetaTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_memory_functions_list_with_versions_attached = {

      luaL_Reg_With_Version({"readFrom", "1.0", DoGeneralRead}),
      luaL_Reg_With_Version({"readUnsignedBytes", "1.0", DoReadUnsignedBytes}),
      luaL_Reg_With_Version({"readSignedBytes", "1.0", DoReadSignedBytes}),
      luaL_Reg_With_Version({"readFixedLengthString", "1.0", DoReadFixedLengthString}),
      luaL_Reg_With_Version({"readNullTerminatedString", "1.0", DoReadNullTerminatedString}),

      luaL_Reg_With_Version({"writeTo", "1.0", DoGeneralWrite}),
      luaL_Reg_With_Version({"writeBytes", "1.0", DoWriteBytes}),
      luaL_Reg_With_Version({"writeString", "1.0", DoWriteString})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_memory_functions_list_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, class_name);
}

// The following functions are helper functions which are only used in this file:
u8 ReadU8FromDomainFunction(lua_State* lua_state, u32 address)
{
  std::optional<PowerPC::ReadResult<u8>> read_result = PowerPC::HostTryReadU8(address);
  if (!read_result.has_value())
  {
    // luaL_error(lua_state, "Error: Attempt to read_u8 from memory failed!");
    return 0;  // TODO: Change this value back to 1, and uncomment out the line above.
  }
  return read_result.value().value;
}

u16 ReadU16FromDomainFunction(lua_State* lua_state, u32 address)
{
  std::optional<PowerPC::ReadResult<u16>> read_result = PowerPC::HostTryReadU16(address);
  if (!read_result.has_value())
  {
    luaL_error(lua_state, "Error: Attempt to read_u16 from memory failed!");
    return 1;
  }
  return read_result.value().value;
}

u32 ReadU32FromDomainFunction(lua_State* lua_state, u32 address)
{
  std::optional<PowerPC::ReadResult<u32>> read_result = PowerPC::HostTryReadU32(address);
  if (!read_result.has_value())
  {
    luaL_error(lua_state, "Error: Attempt to read_u32 from memory failed!");
    return 1;
  }
  return read_result.value().value;
}

u64 ReadU64FromDomainFunction(lua_State* lua_state, u32 address)
{
  std::optional<PowerPC::ReadResult<u64>> read_result = PowerPC::HostTryReadU64(address);
  if (!read_result.has_value())
  {
    luaL_error(lua_state, "Error: Attempt to read_u64 from memory failed!");
    return 1;
  }
  return read_result.value().value;
}

std::string GetStringFromDomainFunction(lua_State* lua_state, u32 address, u32 length)
{
  std::optional<PowerPC::ReadResult<std::string>> read_result =
      PowerPC::HostTryReadString(address, length);
  if (!read_result.has_value())
  {
    luaL_error(lua_state, "Error: Attempt to read string from memory failed!");
    return "";
  }
  return read_result.value().value;
}

u8* GetPointerToDomainFunction(lua_State* lua_state, u32 address)
{
  return Core::System::GetInstance().GetMemory().GetPointer(address);
}

void WriteU8ToDomainFunction(lua_State* lua_state, u32 address, u8 val)
{
  std::optional<PowerPC::WriteResult> write_result = PowerPC::HostTryWriteU8(val, address);
  if (!write_result.has_value())
    luaL_error(lua_state, "Error: Attempt to write_u8 to memory failed!");
}

void WriteU16ToDomainFunction(lua_State* lua_state, u32 address, u16 val)
{
  std::optional<PowerPC::WriteResult> write_result = PowerPC::HostTryWriteU16(val, address);
  if (!write_result.has_value())
    luaL_error(lua_state, "Error: Attempt to write_u16 to memory failed!");
}

void WriteU32ToDomainFunction(lua_State* lua_state, u32 address, u32 val)
{
  std::optional<PowerPC::WriteResult> write_result = PowerPC::HostTryWriteU32(val, address);
  if (!write_result.has_value())
    luaL_error(lua_state, "Error: Attempt to write_u32 to memory failed!");
}

void WriteU64ToDomainFunction(lua_State* lua_state, u32 address, u64 val)
{
  std::optional<PowerPC::WriteResult> write_result = PowerPC::HostTryWriteU64(val, address);
  if (!write_result.has_value())
    luaL_error(lua_state, "Error: Attempt to write_u64 to memory failed!");
}

void CopyStringToDomainFunction(lua_State* lua_state, u32 address, const char* string_to_write,
                                size_t string_length)
{
  Core::System::GetInstance().GetMemory().CopyToEmu(address, string_to_write, string_length + 1);
}

u8 ReadU8(lua_State* lua_state, u32 address)
{
  return ReadU8FromDomainFunction(lua_state, address);
}

s8 ReadS8(lua_State* lua_state, u32 address)
{
  u8 raw_val = ReadU8FromDomainFunction(lua_state, address);
  s8 signed_byte = 0;
  memcpy(&signed_byte, &raw_val, sizeof(s8));
  return signed_byte;
}

u16 ReadU16(lua_State* lua_state, u32 address)
{
  return ReadU16FromDomainFunction(lua_state, address);
}

s16 ReadS16(lua_State* lua_state, u32 address)
{
  u16 raw_val = ReadU16FromDomainFunction(lua_state, address);
  s16 signed_short = 0;
  memcpy(&signed_short, &raw_val, sizeof(s16));
  return signed_short;
}

u32 ReadU32(lua_State* lua_state, u32 address)
{
  return ReadU32FromDomainFunction(lua_state, address);
}

s32 ReadS32(lua_State* lua_state, u32 address)
{
  u32 raw_val = ReadU32FromDomainFunction(lua_state, address);
  s32 signed_int = 0;
  memcpy(&signed_int, &raw_val, sizeof(s32));
  return signed_int;
}

s64 ReadS64(lua_State* lua_state, u32 address)
{
  u64 raw_val = ReadU64FromDomainFunction(lua_state, address);
  s64 signed_long_long = 0;
  memcpy(&signed_long_long, &raw_val, sizeof(s64));
  return signed_long_long;
}

float ReadFloat(lua_State* lua_state, u32 address)
{
  u32 raw_val = ReadU32FromDomainFunction(lua_state, address);
  float my_float = 0.0;
  memcpy(&my_float, &raw_val, sizeof(float));
  return my_float;
}

double ReadDouble(lua_State* lua_state, u32 address)
{
  u64 raw_val = ReadU64FromDomainFunction(lua_state, address);
  double my_double = 0.0;
  memcpy(&my_double, &raw_val, sizeof(double));
  return my_double;
}

void WriteU8(lua_State* lua_state, u32 address, u8 value)
{
  WriteU8ToDomainFunction(lua_state, address, value);
}

void WriteS8(lua_State* lua_state, u32 address, s8 value)
{
  u8 unsigned_byte = 0;
  memcpy(&unsigned_byte, &value, sizeof(s8));
  WriteU8ToDomainFunction(lua_state, address, unsigned_byte);
}

void WriteU16(lua_State* lua_state, u32 address, u16 value)
{
  WriteU16ToDomainFunction(lua_state, address, value);
}

void WriteS16(lua_State* lua_state, u32 address, s16 value)
{
  u16 unsigned_short = 0;
  memcpy(&unsigned_short, &value, sizeof(s16));
  WriteU16ToDomainFunction(lua_state, address, unsigned_short);
}

void WriteU32(lua_State* lua_state, u32 address, u32 value)
{
  WriteU32ToDomainFunction(lua_state, address, value);
}

void WriteS32(lua_State* lua_state, u32 address, s32 value)
{
  u32 unsigned_int = 0;
  memcpy(&unsigned_int, &value, sizeof(s32));
  WriteU32ToDomainFunction(lua_state, address, unsigned_int);
}

void WriteS64(lua_State* lua_state, u32 address, s64 value)
{
  u64 unsigned_long_long = 0;
  memcpy(&unsigned_long_long, &value, sizeof(s64));
  WriteU64ToDomainFunction(lua_state, address, unsigned_long_long);
}

void WriteFloat(lua_State* lua_state, u32 address, float value)
{
  u32 unsigned_int = 0UL;
  memcpy(&unsigned_int, &value, sizeof(float));
  WriteU32ToDomainFunction(lua_state, address, unsigned_int);
}

void WriteDouble(lua_State* lua_state, u32 address, double value)
{
  u64 unsigned_long_long = 0ULL;
  memcpy(&unsigned_long_long, &value, sizeof(double));
  WriteU64ToDomainFunction(lua_state, address, unsigned_long_long);
}

// End of helper functions block.

int DoGeneralRead(lua_State* lua_state)  // format is: 1st argument after object is address,
                                         // 2nd argument after object is type string.
{
  const char* func_name = "readFrom";

  u8 u8_val = 0;
  u16 u16_val = 0;
  u32 u32_val = 0LL;
  s8 s8_val = 0;
  s16 s16_val = 0;
  s32 s32_val = 0;
  s64 s64_val = 0ULL;
  float float_val = 0.0;
  double double_val = 0.0;

  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(0X80000043, \"u32\")");

  u32 address = luaL_checkinteger(lua_state, 2);
  const char* type_string = luaL_checkstring(lua_state, 3);

  switch (ParseType(type_string))
  {
  case NumberType::Unsigned8:
    u8_val = ReadU8(lua_state, address);
    lua_pushinteger(lua_state, static_cast<lua_Integer>(u8_val));
    return 1;

  case NumberType::Unsigned16:
    u16_val = ReadU16(lua_state, address);
    lua_pushinteger(lua_state, static_cast<lua_Integer>(u16_val));
    return 1;

  case NumberType::Unsigned32:
    u32_val = ReadU32(lua_state, address);
    lua_pushinteger(lua_state, static_cast<lua_Integer>(u32_val));
    return 1;

  case NumberType::Unsigned64:
    luaL_error(
        lua_state,
        fmt::format(
            "Error: in {}:{}(), attempted to read from address as UNSIGNED_64. The largest "
            "Lua type is SIGNED_64. As such, this should be used for 64 bit integers instead.",
            class_name, func_name)
            .c_str());
    return 1;

  case NumberType::Signed8:
    s8_val = ReadS8(lua_state, address);
    lua_pushinteger(lua_state, static_cast<lua_Integer>(s8_val));
    return 1;

  case NumberType::Signed16:
    s16_val = ReadS16(lua_state, address);
    lua_pushinteger(lua_state, static_cast<lua_Integer>(s16_val));
    return 1;

  case NumberType::Signed32:
    s32_val = ReadS32(lua_state, address);
    lua_pushinteger(lua_state, static_cast<lua_Integer>(s32_val));
    return 1;

  case NumberType::Signed64:
    s64_val = ReadS64(lua_state, address);
    lua_pushnumber(lua_state, static_cast<lua_Number>(s64_val));
    return 1;

  case NumberType::Float:
    float_val = ReadFloat(lua_state, address);
    lua_pushnumber(lua_state, static_cast<lua_Number>(float_val));
    return 1;

  case NumberType::Double:
    double_val = ReadDouble(lua_state, address);
    lua_pushnumber(lua_state, static_cast<lua_Number>(double_val));
    return 1;

  default:
    luaL_error(
        lua_state,
        fmt::format("Error: Undefined type encountered in {}:{}() function. Valid types are "
                    "u8, u16, u32, s8, s16, s32, s64, float, double, unsigned_8, unsigned_16, "
                    "unsigned_32, signed_8, signed_16, signed_32, signed_64, unsigned "
                    "byte, signed byte, unsigned int, signed int, signed long "
                    "long",
                    class_name, func_name)
            .c_str());
    return 0;
  }
}

int DoReadUnsignedBytes(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "readUnsignedBytes", "(0X80000456, 44)");
  u32 address = luaL_checkinteger(lua_state, 2);
  u32 num_bytes = luaL_checkinteger(lua_state, 3);
  u8* pointer_to_base_address = GetPointerToDomainFunction(lua_state, address);
  lua_createtable(lua_state, num_bytes, 0);
  for (size_t i = 0; i < num_bytes; ++i)
  {
    u8 curr_byte = pointer_to_base_address[i];
    lua_pushinteger(lua_state, static_cast<lua_Integer>(curr_byte));
    lua_rawseti(lua_state, -2, address + i);
  }
  return 1;
}

int DoReadSignedBytes(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "readSignedBytes", "(0X800000065, 44)");
  u32 address = luaL_checkinteger(lua_state, 2);
  u32 num_bytes = luaL_checkinteger(lua_state, 3);
  u8* pointer_to_base_address = GetPointerToDomainFunction(lua_state, address);
  lua_createtable(lua_state, num_bytes, 0);
  for (size_t i = 0; i < num_bytes; ++i)
  {
    s8 current_byte_signed = 0;
    memcpy(&current_byte_signed, pointer_to_base_address + i, sizeof(s8));
    lua_pushinteger(lua_state, static_cast<lua_Integer>(current_byte_signed));
    lua_rawseti(lua_state, -2, address + i);
  }
  return 1;
}

int DoReadFixedLengthString(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "readFixedLengthString", "(0X8000043, 52)");
  u32 address = luaL_checkinteger(lua_state, 2);
  u32 string_length = luaL_checkinteger(lua_state, 3);
  if (string_length == 0)
  {
    lua_pushfstring(lua_state, "");
  }
  else
  {
    std::string return_result = GetStringFromDomainFunction(lua_state, address, string_length);
    lua_pushfstring(lua_state, return_result.c_str());
  }
  return 1;
}

int DoReadNullTerminatedString(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "readNullTermiantedString", "(0X80000043)");
  u32 address = luaL_checkinteger(lua_state, 2);
  u8* memory_pointer_start = GetPointerToDomainFunction(lua_state, address);
  u8* memory_pointer_current = memory_pointer_start;

  while (*memory_pointer_current != '\0')
    memory_pointer_current++;

  if (memory_pointer_current == memory_pointer_start)
  {
    lua_pushfstring(lua_state, "");
  }
  else
  {
    std::string return_result = GetStringFromDomainFunction(
        lua_state, address, (u32)(memory_pointer_current - memory_pointer_start));
    lua_pushfstring(lua_state, return_result.c_str());
  }
  return 1;
}

int DoGeneralWrite(lua_State* lua_state)
{
  const char* func_name = "writeTo";

  u8 u8_val = 0;
  s8 s8_val = 0;
  u16 u16_val = 0;
  s16 s16_val = 0;
  u32 u32_val = 0;
  s32 s32_val = 0;
  float float_val = 0.0;
  s64 s64_val;
  double double_val = 0;

  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(0X8000045, \"u32\", 34000)");

  u32 address = luaL_checkinteger(lua_state, 2);
  const char* type_string = luaL_checkstring(lua_state, 3);

  switch (ParseType(type_string))
  {
  case NumberType::Unsigned8:
    u8_val = luaL_checkinteger(lua_state, 4);
    WriteU8(lua_state, address, u8_val);
    return 0;

  case NumberType::Signed8:
    s8_val = luaL_checkinteger(lua_state, 4);
    WriteS8(lua_state, address, s8_val);
    return 0;

  case NumberType::Unsigned16:
    u16_val = luaL_checkinteger(lua_state, 4);
    WriteU16(lua_state, address, u16_val);
    return 0;

  case NumberType::Signed16:
    s16_val = luaL_checkinteger(lua_state, 4);
    WriteS16(lua_state, address, s16_val);
    return 0;

  case NumberType::Unsigned32:
    u32_val = luaL_checkinteger(lua_state, 4);
    WriteU32(lua_state, address, u32_val);
    return 0;

  case NumberType::Signed32:
    s32_val = luaL_checkinteger(lua_state, 4);
    WriteS32(lua_state, address, s32_val);
    return 0;

  case NumberType::Float:
    float_val = static_cast<float>(luaL_checknumber(lua_state, 4));
    WriteFloat(lua_state, address, float_val);
    return 0;

  case NumberType::Unsigned64:
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}(), attempted to write to address an "
                           "UNSIGNED_64. However, SIGNED_64 is the largest type Lua supports. This "
                           "should be used whenever you want to represent a 64 bit integer.",
                           class_name, func_name)
                   .c_str());
    return 0;

  case NumberType::Signed64:
    s64_val = luaL_checknumber(lua_state, 4);
    WriteS64(lua_state, address, s64_val);
    return 0;

  case NumberType::Double:
    double_val = luaL_checknumber(lua_state, 4);
    WriteDouble(lua_state, address, double_val);
    return 0;

  default:
    luaL_error(lua_state, fmt::format("Error: undefined type passed into {}:{}() method. Valid types "
                          "include u8, u16, u32, s8, s16, s32, s64, float, double.", class_name, func_name).c_str());
    return 0;
  }
}

int DoWriteBytes(lua_State* lua_state)
{
  const char* func_name = "writeBytes";
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(arrayName)");
  lua_pushnil(lua_state); /* first key */
  while (lua_next(lua_state, 2) != 0)
  {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    u32 address = luaL_checkinteger(lua_state, -2);
    s32 value = luaL_checkinteger(lua_state, -1);

    if (value < -128 || value > 255)
    {
      luaL_error(
          lua_state,
          fmt::format("Error: Invalid number passed into {}:{}() function. In order to "
                      "be representable as a byte, the value must be between -128 and 255. "
                      "However, the number which was supposed to be written to address {:x} was {}",
                      class_name, func_name, address, value)
              .c_str());
      return 0;
    }
    if (value < 0)
    {
      s8 s8_val = value;
      WriteS8(lua_state, address, s8_val);
    }
    else
    {
      u8 u8_val = value;
      WriteU8(lua_state, address, u8_val);
    }
    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(lua_state, 1);
  }
  return 0;
}

int DoWriteString(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "writeString", "(0X80000043, \"exampleString\")");
  u32 address = luaL_checkinteger(lua_state, 2);
  const char* string_to_write = luaL_checkstring(lua_state, 3);
  size_t string_size = strlen(string_to_write);
  CopyStringToDomainFunction(lua_state, address, string_to_write, string_size);
  return 0;
}

}  // namespace Lua::LuaMemoryApi
