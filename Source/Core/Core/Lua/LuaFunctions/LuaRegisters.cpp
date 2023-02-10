#include "LuaRegisters.h"

#include <algorithm>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/Lua/LuaHelperClasses/NumberType.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/PowerPC/PowerPC.h"

namespace Lua::LuaRegisters
{
class LuaRegister
{
public:
  inline LuaRegister() {}
};

LuaRegister* lua_register_pointer = nullptr;

LuaRegister* GetLuaRegisterInstance()
{
  if (lua_register_pointer == nullptr)
    lua_register_pointer = new LuaRegister();
  return lua_register_pointer;
}

// Currently supported registers are:
// r0 - r31, f0 - f31, PC, and LR (the register which stores the return address to jump to when a
// function call ends)
void InitLuaRegistersFunctions(lua_State* lua_state, const std::string& lua_api_version)
{
  LuaRegister** lua_register_ptr_ptr =
      (LuaRegister**)lua_newuserdata(lua_state, sizeof(LuaRegister*));
  *lua_register_ptr_ptr = GetLuaRegisterInstance();
  luaL_newmetatable(lua_state, "LuaRegistersTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_registers_functions_with_versions_attached = {
      luaL_Reg_With_Version({"getRegister", "1.0", GetRegister}),
      luaL_Reg_With_Version(
          {"getRegisterAsUnsignedByteArray", "1.0", GetRegisterAsUnsignedByteArray}),
      luaL_Reg_With_Version({"getRegisterAsSignedByteArray", "1.0", GetRegisterAsSignedByteArray}),
      luaL_Reg_With_Version({"setRegister", "1.0", SetRegister}),
      luaL_Reg_With_Version({"setRegisterFromByteArray", "1.0", SetRegisterFromByteArray})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;

  AddLatestFunctionsForVersion(lua_registers_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, "registers");
}

class RegisterObject
{
public:
  enum class RegisterType
  {
    GeneralPurposeRegister,
    FloatingPointRegister,
    PcRegister,
    ReturnRegister,  // LR register
    Undefined
  };

  RegisterObject(RegisterType new_register_type, u8 new_register_number)
  {
    register_type = new_register_type;
    register_number = new_register_number;
  }

  u8 register_number;
  RegisterType register_type;
};

RegisterObject ParseRegister(const char* register_string)
{
  if (register_string == nullptr || register_string[0] == '\0')
    return RegisterObject(RegisterObject::RegisterType::Undefined, 0);

  s64 register_number = 0;
  switch (register_string[0])
  {
  case 'r':
  case 'R':

    register_number = std::stoi(std::string(register_string).substr(1), nullptr);
    if (register_number < 0 || register_number > 31)
      return RegisterObject(RegisterObject::RegisterType::Undefined, 0);
    return RegisterObject(RegisterObject::RegisterType::GeneralPurposeRegister, register_number);
    break;

  case 'f':
  case 'F':
    register_number = std::stoi(std::string(register_string).substr(1), nullptr);
    if (register_number < 0 || register_number > 31)
      return RegisterObject(RegisterObject::RegisterType::Undefined, 0);
    return RegisterObject(RegisterObject::RegisterType::FloatingPointRegister, register_number);
    break;

  case 'p':
  case 'P':
    if (register_string[1] != 'c' && register_string[1] != 'C')
      return RegisterObject(RegisterObject::RegisterType::Undefined, 0);
    return RegisterObject(RegisterObject::RegisterType::PcRegister, 0);
    break;

  case 'l':
  case 'L':
    if (register_string[1] != 'r' && register_string[1] != 'R')
      return RegisterObject(RegisterObject::RegisterType::Undefined, 0);
    return RegisterObject(RegisterObject::RegisterType::ReturnRegister, 0);
    break;

  default:
    return RegisterObject(RegisterObject::RegisterType::Undefined, 0);
  }
}

u8* GetAddressForRegister(RegisterObject register_object, lua_State* lua_state,
                          const char* func_name)
{
  u8 register_number = 0;
  u8* address = nullptr;
  switch (register_object.register_type)
  {
  case RegisterObject::RegisterType::GeneralPurposeRegister:
    register_number = register_object.register_number;
    address = (u8*)(PowerPC::ppcState.gpr + register_number);
    return address;
  case RegisterObject::RegisterType::PcRegister:
    address = ((u8*)&PowerPC::ppcState.pc);
    return address;
  case RegisterObject::RegisterType::ReturnRegister:
    address = ((u8*)(&PowerPC::ppcState.spr[SPR_LR]));
  case RegisterObject::RegisterType::FloatingPointRegister:
    address = (u8*)(PowerPC::ppcState.ps + register_number);
    return address;

  default:
    luaL_error(lua_state,
               (std::string("Error: Invalid string passed in for register name in ") + func_name +
                " function. Currently, R0-R31, F0-F31, PC, and LR are the only registers which are "
                "supported.")
                   .c_str());
    return nullptr;
  }
}

void PushValueFromAddress(lua_State* lua_state, u8* memory_location, NumberType return_type,
                          u8 register_size, u8 offset_bytes)
{
  u8 return_type_size = GetMaxSize(return_type);
  if (return_type_size > register_size)
    luaL_error(
        lua_state,
        "Error: in getRegister(), user requested a return type that was larger than the register!");
  else if (return_type_size + offset_bytes > register_size)
    luaL_error(lua_state, "Error: in getRegister(), there was not enough room after offsetting the "
                          "specified number of bytes in order to get the specified data type.");

  u8 unsigned8 = 0;
  s8 signed8 = 0;
  u16 unsigned16 = 0;
  s16 signed16 = 0;
  u32 unsigned32 = 0;
  s32 signed32 = 0;
  float float_val = 0.0;
  s64 signed64 = 0;
  double double_val = 0.0;

  switch (return_type)
  {
  case NumberType::Unsigned8:
    memcpy(&unsigned8, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, unsigned8);
    return;
  case NumberType::Signed8:
    memcpy(&signed8, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, signed8);
    return;
  case NumberType::Unsigned16:
    memcpy(&unsigned16, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, unsigned16);
    return;
  case NumberType::Signed16:
    memcpy(&signed16, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, signed16);
    return;
  case NumberType::Unsigned32:
    memcpy(&unsigned32, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, unsigned32);
    return;
  case NumberType::Signed32:
    memcpy(&signed32, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, signed32);
    return;
  case NumberType::Float:
    memcpy(&float_val, memory_location + offset_bytes, return_type_size);
    lua_pushnumber(lua_state, float_val);
    return;
  case NumberType::Unsigned64:
    luaL_error(
        lua_state,
        "Error: in getRegister(), UNSIGNED_64 is not a valid type, since Lua stores everything as "
        "SIGNED 64 bit numbers and doubles internally. You should request an S64 instead");
    return;
  case NumberType::Signed64:
    memcpy(&signed64, memory_location + offset_bytes, return_type_size);
    lua_pushinteger(lua_state, signed64);
    return;
  case NumberType::Double:
    memcpy(&double_val, memory_location + offset_bytes, return_type_size);
    lua_pushnumber(lua_state, double_val);
    return;
  default:
    luaL_error(lua_state, "Error: Unknown type string passed in to getRegister() function");
  }
}

int GetRegister(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "getRegister", "registers:getRegister(\"r4\", \"u32\", 4)");
  const char* register_string = luaL_checkstring(lua_state, 2);
  RegisterObject register_object = ParseRegister(register_string);

  const char* return_type_string = luaL_checkstring(lua_state, 3);
  NumberType return_type = ParseType(return_type_string);

  if (return_type == NumberType::Undefined)
    luaL_error(lua_state,
               "Error: undefined type string was passed as an argument to getRegister()");
  s64 offset_bytes = 0;
  u8 register_size = 4;
  if (register_object.register_type == RegisterObject::RegisterType::FloatingPointRegister)
    register_size = 16;
  if (lua_gettop(lua_state) >= 4)
    offset_bytes = luaL_checkinteger(lua_state, 4);
  if (offset_bytes < 0 || offset_bytes > 15)
    luaL_error(lua_state, "Error: in getRegister(), number of bytes to offset from left was either "
                          "less than 0 or more than 15");
  u8* address = GetAddressForRegister(register_object, lua_state, "getRegister()");
  PushValueFromAddress(lua_state, address, return_type, register_size, offset_bytes);

  return 1;
}

int PushByteArrayFromAddressHelperFunction(lua_State* lua_state, bool is_unsigned,
                                           const char* func_name)
{
  const char* register_string = luaL_checkstring(lua_state, 2);
  RegisterObject register_object = ParseRegister(register_string);
  u8 register_size = 4;
  if (register_object.register_type == RegisterObject::RegisterType::FloatingPointRegister)
    register_size = 16;

  u8* address = GetAddressForRegister(register_object, lua_state, func_name);
  s64 array_size = luaL_checkinteger(lua_state, 3);
  if (array_size <= 0)
    luaL_error(lua_state,
               (std::string("Error: in ") + func_name + ", arraySize was <= 0.").c_str());
  s64 offset_bytes = 0;
  if (lua_gettop(lua_state) >= 4)
    offset_bytes = luaL_checkinteger(lua_state, 4);
  if (offset_bytes < 0)
    luaL_error(lua_state,
               (std::string("Error: in ") + func_name + ", offset value was less than 0.").c_str());
  lua_createtable(lua_state, array_size, 0);
  s8 signed8 = 0;
  u8 unsigned8 = 0;

  if (array_size + offset_bytes > register_size)
    luaL_error(lua_state, (std::string("Error: in ") + func_name +
                           ", attempt to read past the end of the register occured.")
                              .c_str());

  for (int i = 0; i < array_size; ++i)
  {
    if (is_unsigned)
    {
      memcpy(&unsigned8, address + offset_bytes + i, 1);
      lua_pushinteger(lua_state, static_cast<lua_Integer>(unsigned8));
    }

    else
    {
      memcpy(&signed8, address + offset_bytes + i, 1);
      lua_pushinteger(lua_state, static_cast<lua_Integer>(signed8));
    }

    lua_rawseti(lua_state, -2, i + 1);
  }
  return 1;
}

int GetRegisterAsUnsignedByteArray(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "getRegisterAsUnsignedByteArray",
                            "registers:getRegisterAsUnsignedByteArray(\"r4\", 2, 6)");
  return PushByteArrayFromAddressHelperFunction(lua_state, true,
                                                "getRegisterAsUnsignedByteArray()");
}

int GetRegisterAsSignedByteArray(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "getRegisterAsSignedByteArray",
                            "registers:getRegisterAsSignedByteArray(\"r4\", 2, 6)");
  return PushByteArrayFromAddressHelperFunction(lua_state, false, "getRegisterAsSignedByteArray()");
}

void WriteValueToAddress(lua_State* lua_state, u8* memory_location, NumberType value_type,
                         u8 register_size, u8 offset_bytes)
{
  u8 value_type_size = GetMaxSize(value_type);
  if (value_type_size > register_size)
    luaL_error(
        lua_state,
        "Error: in setRegister(), user tried to write a number that was larger than the register!");
  else if (offset_bytes >= register_size)
    luaL_error(
        lua_state,
        "Error: in setRegister(), user tried to use an offset that was larger than the register!");
  else if (value_type_size + offset_bytes > register_size)
    luaL_error(lua_state, "Error: in setRegister(), there was not enough room after offsetting the "
                          "specified number of bytes in order to write the specified value.");

  u8 unsigned8 = 0;
  s8 signed8 = 0;
  u16 unsigned16 = 0;
  s16 signed16 = 0;
  u32 unsigned32 = 0;
  s32 signed32 = 0;
  float float_val = 0.0;
  s64 signed64 = 0;
  double double_val = 0.0;

  switch (value_type)
  {
  case NumberType::Unsigned8:
    unsigned8 = static_cast<u8>(luaL_checkinteger(lua_state, 4));
    memcpy(memory_location + offset_bytes, &unsigned8, 1);
    return;
  case NumberType::Signed8:
    signed8 = static_cast<s8>(luaL_checkinteger(lua_state, 4));
    memcpy(memory_location + offset_bytes, &signed8, 1);
    return;
  case NumberType::Unsigned16:
    unsigned16 = static_cast<u16>(luaL_checkinteger(lua_state, 4));
    memcpy(memory_location + offset_bytes, &unsigned16, 2);
    return;
  case NumberType::Signed16:
    signed16 = static_cast<s16>(luaL_checkinteger(lua_state, 4));
    memcpy(memory_location + offset_bytes, &signed16, 2);
    return;
  case NumberType::Unsigned32:
    unsigned32 = static_cast<u32>(luaL_checkinteger(lua_state, 4));
    memcpy(memory_location + offset_bytes, &unsigned32, 4);
    return;
  case NumberType::Signed32:
    signed32 = static_cast<s32>(luaL_checkinteger(lua_state, 4));
    memcpy(memory_location + offset_bytes, &signed32, 4);
    return;
  case NumberType::Unsigned64:
    luaL_error(lua_state,
               "Error: UNSIGNED_64 is not a valid type for setRegister(), since Lua only "
               "supports SIGNED_64 and DOUBLE internally. Please use S64 instead");
    return;
  case NumberType::Signed64:
    signed64 = luaL_checkinteger(lua_state, 4);
    memcpy(memory_location + offset_bytes, &signed64, 8);
    return;
  case NumberType::Float:
    float_val = static_cast<float>(luaL_checknumber(lua_state, 4));
    memcpy(memory_location + offset_bytes, &float_val, 4);
    return;
  case NumberType::Double:
    double_val = luaL_checknumber(lua_state, 4);
    memcpy(memory_location + offset_bytes, &double_val, 8);
    return;
  default:
    luaL_error(lua_state, "Error: Undefined type encountered in setRegister() function");
  }
}

int SetRegister(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "setRegister",
                            "registers:setRegister(\"r4\", \"u32\", 45, 4)");
  const char* register_string = luaL_checkstring(lua_state, 2);
  RegisterObject register_object = ParseRegister(register_string);
  const char* type_string = luaL_checkstring(lua_state, 3);
  NumberType value_type = ParseType(type_string);
  if (value_type == NumberType::Undefined)
    luaL_error(lua_state,
               "Error: undefined type string was passed as an argument to setRegister()");
  u8 register_size = 4;
  if (register_object.register_type == RegisterObject::RegisterType::FloatingPointRegister)
    register_size = 16;
  u8* address = GetAddressForRegister(register_object, lua_state, "setRegister()");
  u8 offset_bytes = 0;
  if (lua_gettop(lua_state) >= 5)
    offset_bytes = luaL_checkinteger(lua_state, 5);
  if (offset_bytes < 0 || offset_bytes > 15)
    luaL_error(lua_state, "Error: in setRegister(), number of bytes to offset from left was either "
                          "less than 0 or more than 15");

  WriteValueToAddress(lua_state, address, value_type, register_size, offset_bytes);

  return 0;
}

struct IndexValuePair
{
  s64 index;
  s64 value;
};

bool SortIndexValuePairByIndexFunction(IndexValuePair o1, IndexValuePair o2)
{
  return o1.index < o2.index;
}

int SetRegisterFromByteArray(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "setRegisterFromByteArray",
                            "registers:setRegisterFromByteArray(\"r4\", byteTable, 2)");
  const char* register_string = luaL_checkstring(lua_state, 2);
  RegisterObject register_object = ParseRegister(register_string);
  u8 register_size = 4;
  if (register_object.register_type == RegisterObject::RegisterType::FloatingPointRegister)
    register_size = 16;
  s64 offset_bytes = 0;
  if (lua_gettop(lua_state) >= 4)
    offset_bytes = luaL_checkinteger(lua_state, 4);
  if (offset_bytes >= register_size)
  {
    luaL_error(
        lua_state,
        "Error: in setRegisterFromByteArray(), size of offset was too large for the register!");
  }

  u8* address = GetAddressForRegister(register_object, lua_state, "setRegisterFromByteArray()");

  s64 signed64 = 0;
  s64 temp_key = 0;
  s8 signed8 = 0;
  u8 unsigned8 = 0;

  std::vector<IndexValuePair> sorted_byte_table = std::vector<IndexValuePair>();
  lua_pushnil(lua_state);
  while (lua_next(lua_state, 3) != 0)
  {
    temp_key = lua_tointeger(lua_state, -2);
    signed64 = lua_tointeger(lua_state, -1);
    sorted_byte_table.push_back(IndexValuePair(temp_key, signed64));

    lua_pop(lua_state, 1);
  }

  std::sort(sorted_byte_table.begin(), sorted_byte_table.end(), SortIndexValuePairByIndexFunction);

  for (int i = 0; i < sorted_byte_table.size(); ++i)
  {
    if (i + 1 + offset_bytes > register_size)
    {
      luaL_error(lua_state, "Error: in setRegisterFromByteArray(), attempt to write byte array "
                            "past the end of the register occured!");
    }

    signed64 = sorted_byte_table[i].value;

    if (signed64 < -128)
      luaL_error(lua_state,
                 "Error: in setRegisterFromByteArray(), attempted to write value from byte array "
                 "which could not be represented with 1 byte (value was less than -128)");
    else if (signed64 > 255)
      luaL_error(lua_state,
                 "Error: in setRegisterFromByteArray(), attempted to write value from byte array "
                 "which could not be represented with 1 byte (value was greater than 255)");

    if (signed64 < 0)
    {
      signed8 = static_cast<s8>(signed64);
      memcpy(address + offset_bytes + i, &signed8, 1);
    }
    else
    {
      unsigned8 = static_cast<u8>(signed64);
      memcpy(address + offset_bytes + i, &unsigned8, 1);
    }
  }
  return 0;
}

}  // namespace Lua::LuaRegisters
