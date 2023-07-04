#include "Core/Scripting/InternalAPIModules/MemoryAPI.h"

#include <fmt/format.h>
#include <fstream>
#include <optional>

#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "Core/System.h"

namespace Scripting::MemoryApi
{
const char* class_name = "MemoryAPI";

static std::array all_memory_functions_metadata_list = {
    FunctionMetadata("read_u8", "1.0", "read_u8(0X80003421)", ReadU8,
                     ScriptingEnums::ArgTypeEnum::U8, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_u16", "1.0", "read_u16(0X80003421)", ReadU16,
                     ScriptingEnums::ArgTypeEnum::U16, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_u32", "1.0", "read_u32(0X80003421)", ReadU32,
                     ScriptingEnums::ArgTypeEnum::U32, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_u64", "1.0", "read_u64(0X80003421)", ReadU64,
                     ScriptingEnums::ArgTypeEnum::U64, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_s8", "1.0", "read_s8(0X80003421)", ReadS8,
                     ScriptingEnums::ArgTypeEnum::S8, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_s16", "1.0", "read_s16(0X80003421)", ReadS16,
                     ScriptingEnums::ArgTypeEnum::S16, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_s32", "1.0", "read_s32(0X80003421)", ReadS32,
                     ScriptingEnums::ArgTypeEnum::S32, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_s64", "1.0", "read_s64(0X80003421)", ReadS64,
                     ScriptingEnums::ArgTypeEnum::S64, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_float", "1.0", "read_float(0X80003421)", ReadFloat,
                     ScriptingEnums::ArgTypeEnum::Float, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_double", "1.0", "read_double(0X80003421)", ReadDouble,
                     ScriptingEnums::ArgTypeEnum::Double, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_fixed_length_string", "1.0", "read_fixed_length_string(0X80003421, 8)",
                     ReadFixedLengthString, ScriptingEnums::ArgTypeEnum::String,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_null_terminated_string", "1.0",
                     "read_null_terminated_string(0X80003421)", ReadNullTerminatedString,
                     ScriptingEnums::ArgTypeEnum::String, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_unsigned_bytes", "1.0", "read_unsigned_bytes(0X80003421, 6)",
                     ReadUnsignedBytes, ScriptingEnums::ArgTypeEnum::AddressToByteMap,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("read_signed_bytes", "1.0", "read_signed_bytes(0X80003421, 6)",
                     ReadSignedBytes, ScriptingEnums::ArgTypeEnum::AddressToByteMap,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),

    FunctionMetadata("write_u8", "1.0", "write_u8(0X80003421, 41)", WriteU8,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::U8}),
    FunctionMetadata("write_u16", "1.0", "write_u16(0X80003421, 400)", WriteU16,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::U16}),
    FunctionMetadata("write_u32", "1.0", "write_u32(0X80003421, 500000)", WriteU32,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::U32}),
    FunctionMetadata("write_u64", "1.0", "write_u64(0X80003421, 7000000)", WriteU64,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::U64}),
    FunctionMetadata("write_s8", "1.0", "write_s8(0X80003421, -42)", WriteS8,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S8}),
    FunctionMetadata("write_s16", "1.0", "write_s16(0X80003421, -500)", WriteS16,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S16}),
    FunctionMetadata("write_s32", "1.0", "write_s32(0X80003421, -100000)", WriteS32,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S32}),
    FunctionMetadata("write_s64", "1.0", "write_s64(0X80003421, -70000000)", WriteS64,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("write_float", "1.0", "write_float(0X80003421, 85.64)", WriteFloat,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::Float}),
    FunctionMetadata("write_double", "1.0", "write_double(0X80003421, 143.51)", WriteDouble,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::Double}),
    FunctionMetadata("write_string", "1.0", "write_string(0X80003421, \"Hello World!\")",
                     WriteString, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("write_bytes", "1.0", "write_bytes(addressToValueMap)", WriteBytes,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::AddressToByteMap}),
    FunctionMetadata("writeAllMemoryAsUnsignedBytesToFile", "1.0",
                     "ReadAllMemoryAsUnsignedBytes(myFileName)",
                     WriteAllMemoryAsUnsignedBytesToFile, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_memory_functions_metadata_list, api_version,
                                                   deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_memory_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_memory_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder* ReadU8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u8>> read_result =
      PowerPC::MMU::HostTryReadU8(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
  {
    // return CreateErrorStringArgHolder(fmt::format("Attempt to read u8 from address {} failed!,
    // address))
    return CreateS32ArgHolder(0);  // TODO: remove this line and uncomment out the line above.
  }
  return CreateU8ArgHolder(read_result.value().value);
}

ArgHolder* ReadU16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u16>> read_result =
      PowerPC::MMU::HostTryReadU16(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read u16 from address {} failed!", address));
  return CreateU16ArgHolder(read_result.value().value);
}

ArgHolder* ReadU32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowd to be negative!");
  std::optional<PowerPC::ReadResult<u32>> read_result =
      PowerPC::MMU::HostTryReadU32(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read u32 from address {} failed!", address));
  return CreateU32ArgHolder(read_result.value().value);
}

ArgHolder* ReadU64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u64>> read_result =
      PowerPC::MMU::HostTryReadU64(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read u64 from address {} failed!", address));
  return CreateU64ArgHolder(read_result.value().value);
}

ArgHolder* ReadS8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u8>> read_result =
      PowerPC::MMU::HostTryReadU8(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read s8 from address {} failed!", address));
  u8 u8_val = read_result.value().value;
  s8 s8_val = 0;
  memcpy(&s8_val, &u8_val, sizeof(s8));
  return CreateS8ArgHolder(s8_val);
}

ArgHolder* ReadS16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u16>> read_result =
      PowerPC::MMU::HostTryReadU16(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read s16 from address {} failed!", address));
  u16 u16_val = read_result.value().value;
  s16 s16_val = 0;
  memcpy(&s16_val, &u16_val, sizeof(s16));
  return CreateS16ArgHolder(s16_val);
}

ArgHolder* ReadS32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u32>> read_result =
      PowerPC::MMU::HostTryReadU32(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read s32 from address {} failed!", address));
  u32 u32_val = read_result.value().value;
  s32 s32_val = 0;
  memcpy(&s32_val, &u32_val, sizeof(s32));
  return CreateS32ArgHolder(s32_val);
}

ArgHolder* ReadS64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u64>> read_result =
      PowerPC::MMU::HostTryReadU64(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read s64 from address {} failed!", address));
  u64 u64_val = read_result.value().value;
  s64 s64_val = 0;
  memcpy(&s64_val, &u64_val, sizeof(s64));
  return CreateS64ArgHolder(s64_val);
}

ArgHolder* ReadFloat(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u32>> read_result =
      PowerPC::MMU::HostTryReadU32(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read float from address {} failed!", address));
  u32 u32_val = read_result.value().value;
  float float_val = 0.0f;
  memcpy(&float_val, &u32_val, sizeof(float));
  return CreateFloatArgHolder(float_val);
}

ArgHolder* ReadDouble(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::optional<PowerPC::ReadResult<u64>> read_result =
      PowerPC::MMU::HostTryReadU64(Core::CPUThreadGuard(Core::System::GetInstance()), address);
  if (!read_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Attempt to read double from address {} failed!", address));
  u64 u64_val = read_result.value().value;
  double double_val = 0.0l;
  memcpy(&double_val, &u64_val, sizeof(double));
  return CreateDoubleArgHolder(double_val);
}

ArgHolder* ReadFixedLengthString(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  long long string_length = (*args_list)[1]->s64_val;

  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  if (string_length < 0)
    return CreateErrorStringArgHolder("Length of string is not allowed to be negative!");
  if (string_length == 0)
    return CreateStringArgHolder("");

  std::string return_string = std::string(string_length, ' ');
  for (long long i = 0; i < string_length; ++i)
  {
    std::optional<PowerPC::ReadResult<u8>> read_result =
        PowerPC::MMU::HostTryReadU8(Core::CPUThreadGuard(Core::System::GetInstance()), address + i);
    if (!read_result.has_value())
      return CreateErrorStringArgHolder(
          fmt::format("Could not read char at address of {}", address + i));
    return_string[i] = static_cast<char>(read_result.value().value);
  }
  return CreateStringArgHolder(return_string);
}

ArgHolder* ReadNullTerminatedString(ScriptContext* current_script,
                                    std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;

  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowd to be negative!");

  std::string returnString = "";

  long long offset = 0;
  while (true)
  {
    std::optional<PowerPC::ReadResult<u8>> read_result = PowerPC::MMU::HostTryReadU8(
        Core::CPUThreadGuard(Core::System::GetInstance()), address + offset);
    if (!read_result.has_value())
      return CreateErrorStringArgHolder(
          fmt::format("Could not read char at address of {}", address + offset));
    char next_char = static_cast<char>(read_result.value().value);
    if (next_char == '\0')
      return CreateStringArgHolder(returnString);
    else
      returnString += next_char;

    ++offset;
  }
  return CreateErrorStringArgHolder("An unknown read error occured");
}

ArgHolder* ReadUnsignedBytes(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  long long number_of_bytes = (*args_list)[1]->s64_val;

  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");

  if (number_of_bytes < 0)
    return CreateErrorStringArgHolder("Number of bytes is not allowed to be negative!");

  std::map<long long, s16> address_to_unsigned_byte_map = std::map<long long, s16>();

  for (long long i = 0; i < number_of_bytes; ++i)
  {
    std::optional<PowerPC::ReadResult<u8>> read_result =
        PowerPC::MMU::HostTryReadU8(Core::CPUThreadGuard(Core::System::GetInstance()), address + i);
    if (!read_result.has_value())
      return CreateErrorStringArgHolder(
          fmt::format("Could not read unsigned byte at address {}", address + i));
    address_to_unsigned_byte_map[address + i] = read_result.value().value;
  }

  return CreateAddressToByteMapArgHolder(address_to_unsigned_byte_map);
}

ArgHolder* ReadSignedBytes(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  long long number_of_bytes = (*args_list)[1]->s64_val;

  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  if (number_of_bytes < 0)
    return CreateErrorStringArgHolder("Number of bytes is not allowed to be negative!");

  std::map<long long, s16> address_to_signed_byte_map = std::map<long long, s16>();

  for (long long i = 0; i < number_of_bytes; ++i)
  {
    std::optional<PowerPC::ReadResult<u8>> read_result =
        PowerPC::MMU::HostTryReadU8(Core::CPUThreadGuard(Core::System::GetInstance()), address + i);
    if (!read_result.has_value())
      return CreateErrorStringArgHolder(
          fmt::format("Could not read signed byte at address {}", address + i));
    s8 s8_val = 0;
    u8 u8_val = read_result.value().value;
    memcpy(&s8_val, &u8_val, sizeof(s8));
    address_to_signed_byte_map[address + i] = s8_val;
  }

  return CreateAddressToByteMapArgHolder(address_to_signed_byte_map);
}

ArgHolder* WriteU8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  u8 u8_val = (*args_list)[1]->u8_val;
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU8(
      Core::CPUThreadGuard(Core::System::GetInstance()), u8_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write u8 of {} to address {}", u8_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteU16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  u16 u16_val = (*args_list)[1]->u16_val;
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU16(
      Core::CPUThreadGuard(Core::System::GetInstance()), u16_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write u16 of {} to address {}", u16_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteU32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  u32 u32_val = (*args_list)[1]->u32_val;
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU32(
      Core::CPUThreadGuard(Core::System::GetInstance()), u32_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write u32 of {} to address {}", u32_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteU64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  u64 u64_val = (*args_list)[1]->u64_val;
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU64(
      Core::CPUThreadGuard(Core::System::GetInstance()), u64_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write u64 of {} to address {}", u64_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteS8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  s8 s8_val = (*args_list)[1]->s8_val;
  u8 u8_val = 0;
  memcpy(&u8_val, &s8_val, sizeof(u8));
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU8(
      Core::CPUThreadGuard(Core::System::GetInstance()), u8_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write s8 of {} to address {}", s8_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteS16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  s16 s16_val = (*args_list)[1]->s16_val;
  u16 u16_val = 0;
  memcpy(&u16_val, &s16_val, sizeof(u16));
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU16(
      Core::CPUThreadGuard(Core::System::GetInstance()), u16_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write s16 of {} to address {}", s16_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteS32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  s32 s32_val = (*args_list)[1]->s32_val;
  u32 u32_val = 0;
  memcpy(&u32_val, &s32_val, sizeof(u32));
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU32(
      Core::CPUThreadGuard(Core::System::GetInstance()), u32_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write s32 of {} to address {}", s32_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteS64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  s64 s64_val = (*args_list)[1]->s64_val;
  u64 u64_val = 0;
  memcpy(&u64_val, &s64_val, sizeof(u64));
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU64(
      Core::CPUThreadGuard(Core::System::GetInstance()), u64_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write s64 of {} to address {}", s64_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteFloat(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  float float_val = (*args_list)[1]->float_val;
  u32 u32_val = 0;
  memcpy(&u32_val, &float_val, sizeof(u32));
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU32(
      Core::CPUThreadGuard(Core::System::GetInstance()), u32_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write float of {} to address {}", float_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteDouble(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  double double_val = (*args_list)[1]->double_val;
  u64 u64_val = 0;
  memcpy(&u64_val, &double_val, sizeof(u64));
  std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU64(
      Core::CPUThreadGuard(Core::System::GetInstance()), u64_val, address);
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write double of {} to address {}", double_val, address));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteString(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long address = (*args_list)[0]->s64_val;
  if (address < 0)
    return CreateErrorStringArgHolder("Address is not allowed to be negative!");
  std::string input_string = (*args_list)[1]->string_val;
  size_t string_length = input_string.length();
  for (int i = 0; i < string_length; ++i)
  {
    std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU8(
        Core::CPUThreadGuard(Core::System::GetInstance()), input_string[i], address + i);
    if (!write_result.has_value())
      return CreateErrorStringArgHolder(
          fmt::format("Could not write char of {} to address {}", input_string[i], address + i));
  }
  std::optional<PowerPC::WriteResult> write_result =
      PowerPC::MMU::HostTryWriteU8(Core::CPUThreadGuard(Core::System::GetInstance()), '\0',
                                   address + static_cast<s32>(string_length));
  if (!write_result.has_value())
    return CreateErrorStringArgHolder(
        fmt::format("Could not write char of {} to address {}", '\0', address + string_length));
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteBytes(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::map<long long, s16> address_to_byte_map = (*args_list)[0]->address_to_byte_map;
  for (auto& it : address_to_byte_map)
  {
    long long address = it.first;
    s16 raw_value = it.second;
    if (address < 0)
      return CreateErrorStringArgHolder("Address was negative!");
    if (raw_value < -128 || raw_value > 255)
      return CreateErrorStringArgHolder(
          fmt::format("Number of {} cannot be converted to a byte!", raw_value));
    if (raw_value < 0)
    {
      s8 raw_s8 = static_cast<s8>(raw_value);
      u8 u8_val = 0;
      memcpy(&u8_val, &raw_s8, sizeof(u8));
      std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU8(
          Core::CPUThreadGuard(Core::System::GetInstance()), u8_val, address);
      if (!write_result.has_value())
        return CreateErrorStringArgHolder(
            fmt::format("Could not write byte of {} to address {}", raw_value, address));
    }
    else
    {
      u8 u8_val = static_cast<u8>(raw_value);
      std::optional<PowerPC::WriteResult> write_result = PowerPC::MMU::HostTryWriteU8(
          Core::CPUThreadGuard(Core::System::GetInstance()), u8_val, address);
      if (!write_result.has_value())
        return CreateErrorStringArgHolder(
            fmt::format("Could not write byte of {} to address {}", raw_value, address));
    }
  }
  return CreateVoidTypeArgHolder();
}

ArgHolder* WriteAllMemoryAsUnsignedBytesToFile(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list)
{
  std::ofstream output_stream((*args_list)[0]->string_val.c_str(),
                              std::ios::trunc | std::ios::binary);
  Memory::MemoryManager& memory = Core::System::GetInstance().GetMemory();
  u8* ram = memory.getRAM_scriptHelper();
  if (ram != nullptr)
    output_stream.write((char*)ram, memory.GetRamSize());

  u8* l1_cache = memory.getL1Cache_scriptHelper();
  if (l1_cache != nullptr)
    output_stream.write((char*)l1_cache, memory.GetL1CacheSize());

  u8* fake_vmem = memory.getFakeVMEM_scriptHelper();
  if (fake_vmem != nullptr)
    output_stream.write((char*)fake_vmem, memory.GetFakeVMemSize());

  u8* ex_ram = memory.getEXRAM_scriptHelper();
  if (ex_ram != nullptr)
    output_stream.write((char*)ex_ram, memory.GetExRamSize());

  output_stream.flush();
  output_stream.close();

  return CreateVoidTypeArgHolder();
}

}  // namespace Scripting::MemoryApi
