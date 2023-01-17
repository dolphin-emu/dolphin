#include "LuaRegisters.h"
#include "../LuaHelperClasses/NumberType.h"
#include "common/CommonTypes.h"
#include "Core/PowerPC/PowerPC.h"
#include <string>

namespace Lua
{
namespace LuaRegisters
{
class luaRegister
{
public:
  inline luaRegister() {}
};

luaRegister* luaRegisterPointer = NULL;

luaRegister* GetLuaRegisterInstance()
{
  if (luaRegisterPointer == NULL)
    luaRegisterPointer = new luaRegister();
  return luaRegisterPointer;
}

// Currently supported registers are:
// r0 - r31, f0 - f31, PC, and LR (the register which stores the return address to jump to when a
// function call ends)
void InitLuaRegistersFunctions(lua_State* luaState)
{
  luaRegister** luaRegisterPtrPtr = (luaRegister**)lua_newuserdata(luaState, sizeof(luaRegister*));
  *luaRegisterPtrPtr = GetLuaRegisterInstance();
  luaL_newmetatable(luaState, "LuaRegistersTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaRegistersFunctions[] = {
    {"getRegister", getRegister},
    {"getRegisterAsUnsignedByteArray", getRegisterAsUnsignedByteArray},
    {"getRegisterAsSignedByteArray", getRegisterAsSignedByteArray},
    {"setRegister",  setRegister},
    {"setRegisterFromByteArray", setRegisterFromByteArray},
    {nullptr, nullptr}
  };
  luaL_setfuncs(luaState, luaRegistersFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "registers");
}

class RegisterObject
{
public:

  enum REGISTER_TYPE
  {
    GENERAL_PURPOSE_REGISTER,
    FLOATING_POINT_REGISTER,
    PC_REGISTER,
    RETURN_REGISTER, //LR register
    UNDEFINED
  };

  RegisterObject(REGISTER_TYPE newRegType, u8 newRegNum) {
    regType = newRegType;
    registerNum = newRegNum;
  }

  u8 registerNum;
  REGISTER_TYPE regType;
};

RegisterObject parseRegister(const char* registerString)
{
  if (registerString == NULL || registerString[0] == '\0')
    return RegisterObject(RegisterObject::REGISTER_TYPE::UNDEFINED, 0);

  s64 regNum = 0;
  switch (registerString[0])
  {
  case 'r':
  case 'R':

    regNum = std::stoi(std::string(registerString).substr(1), NULL);
    if (regNum < 0 || regNum > 31)
      return RegisterObject(RegisterObject::REGISTER_TYPE::UNDEFINED, 0);
    return RegisterObject(RegisterObject::REGISTER_TYPE::GENERAL_PURPOSE_REGISTER, regNum);
    break;

  case 'f':
  case 'F':
    regNum = std::stoi(std::string(registerString).substr(1), NULL);
    if (regNum < 0 || regNum > 31)
      return RegisterObject(RegisterObject::REGISTER_TYPE::UNDEFINED, 0);
    return RegisterObject(RegisterObject::REGISTER_TYPE::FLOATING_POINT_REGISTER, regNum);
    break;

  case 'p':
  case 'P':
    if (registerString[1] != 'c' && registerString[2] != 'C')
      return RegisterObject(RegisterObject::REGISTER_TYPE::UNDEFINED, 0);
    return RegisterObject(RegisterObject::REGISTER_TYPE::PC_REGISTER, 0);
    break;

  case 'l':
  case 'L':
    if (registerString[1] != 'r' && registerString[1] != 'R')
      return RegisterObject(RegisterObject::REGISTER_TYPE::UNDEFINED, 0);
    return RegisterObject(RegisterObject::REGISTER_TYPE::RETURN_REGISTER, 0);
    break;

  default:
    return RegisterObject(RegisterObject::REGISTER_TYPE::UNDEFINED, 0);
  }
}

u8* getAddressForRegister(RegisterObject registerObject, lua_State* luaState, const char* funcName)
{
  u8 regNum = 0;
  u8* address = NULL;
  switch (registerObject.regType)
  {
  case RegisterObject::REGISTER_TYPE::GENERAL_PURPOSE_REGISTER:
    regNum = registerObject.registerNum;
    address = (u8*)(rGPR + regNum);
    return address;
  case RegisterObject::REGISTER_TYPE::PC_REGISTER:
    address = ((u8*)&PC);
    return address;
  case RegisterObject::REGISTER_TYPE::RETURN_REGISTER:
    address = ((u8*)&LR);
    return address;
  case RegisterObject::REGISTER_TYPE::FLOATING_POINT_REGISTER:
    address = (u8*)(PowerPC::ppcState.ps + regNum);
    return address;

  default:
    luaL_error(luaState,
               (std::string("Error: Invalid string passed in for register name in ") + funcName +
                " function. Currently, R0-R31, F0-F31, PC, and LR are the only registers which are "
                "supported.")
                   .c_str());
    return NULL;
  }
}

void pushValueFromAddress(lua_State* luaState, u8* memoryLocation, NumberType returnType, u8 registerSize, u8 offsetBytes)
{
  u8 returnTypeSize = getMaxSize(returnType);
  if (returnTypeSize > registerSize)
    luaL_error(luaState, "Error: in getRegister(), user requested a return type that was larger than the register!");
  else if (returnTypeSize + offsetBytes > registerSize)
    luaL_error(luaState, "Error: in getRegister(), there was not enough room after offsetting the specified number of bytes in order to get the specified data type.");

  u8 unsigned8 = 0;
  s8 signed8 = 0;
  u16 unsigned16 = 0;
  s16 signed16 = 0;
  u32 unsigned32 = 0;
  s32 signed32 = 0;
  float floatVal = 0.0;
  s64 signed64 = 0;
  double doubleVal = 0.0;

  switch (returnType)
  {
  case NumberType::UNSIGNED_8:
    memcpy(&unsigned8, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, unsigned8);
    return;
  case NumberType::SIGNED_8:
    memcpy(&signed8, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed8);
    return;
  case NumberType::UNSIGNED_16:
    memcpy(&unsigned16, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, unsigned16);
    return;
  case NumberType::SIGNED_16:
    memcpy(&signed16, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed16);
    return;
  case NumberType::UNSIGNED_32:
    memcpy(&unsigned32, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, unsigned32);
    return;
  case NumberType::SIGNED_32:
    memcpy(&signed32, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed32);
    return;
  case NumberType::FLOAT:
    memcpy(&floatVal, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushnumber(luaState, floatVal);
    return;
  case NumberType::UNSIGNED_64:
    luaL_error(
        luaState,
        "Error: in getRegister(), UNSIGNED_64 is not a valid type, since Lua stores everything as "
        "SIGNED 64 bit numbers and doubles internally. You should request an S64 instead");
    return;
  case NumberType::SIGNED_64:
    memcpy(&signed64, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed64);
    return;
  case NumberType::DOUBLE:
    memcpy(&doubleVal, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushnumber(luaState, doubleVal);
    return;
  default:
    luaL_error(luaState, "Error: Unknown type string passed in to getRegister() function");
  }
}

int getRegister(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "getRegister", "registers:getRegister(\"r4\", \"u32\", 4)");
  const char* registerString = luaL_checkstring(luaState, 2);
  RegisterObject registerObject = parseRegister(registerString);

  const char* returnTypeString = luaL_checkstring(luaState, 3);
  NumberType returnType = parseType(returnTypeString);

  if (returnType == NumberType::UNDEFINED)
    luaL_error(luaState, "Error: undefined type string was passed as an argument to getRegister()");
  s64 offsetBytes = 0;
  u8 registerSize = 4;
  if (registerObject.regType == RegisterObject::REGISTER_TYPE::FLOATING_POINT_REGISTER)
    registerSize = 8;
  if (lua_gettop(luaState) >= 4)
    offsetBytes = luaL_checkinteger(luaState, 4);
  if (offsetBytes < 0 || offsetBytes > 7)
    luaL_error(luaState, "Error: in getRegister(), number of bytes to offset from left was either less than 0 or more than 7");
  u8* address = getAddressForRegister(registerObject, luaState, "getRegister()");
  pushValueFromAddress(luaState, address, returnType, registerSize, offsetBytes);

  return 1;
}

int pushByteArrayFromAddressHelperFunction(lua_State* luaState, bool isUnsigned, const char* funcName)
{
  const char* registerString = luaL_checkstring(luaState, 2);
  RegisterObject registerObject = parseRegister(registerString);
  u8* address = getAddressForRegister(registerObject, luaState, funcName);
  s64 arraySize = luaL_checkinteger(luaState, 3);
  s64 offsetBytes = 0;
  if (lua_gettop(luaState) >= 4)
    offsetBytes = luaL_checkinteger(luaState, 4);
  lua_createtable(luaState, arraySize, 0);
  s8 signed8 = 0;
  u8 unsigned8 = 0;
  for (int i = 1; i <= arraySize; ++i)
  {
    if (isUnsigned)
    {
      memcpy(&unsigned8, address + offsetBytes + i - 1, 1);
      lua_pushinteger(luaState, static_cast<lua_Integer>(unsigned8));
    }

    else
    {
      memcpy(&signed8, address + offsetBytes + i - 1, 1);
      lua_pushinteger(luaState, static_cast<lua_Integer>(signed8));
    }

    lua_rawseti(luaState, -2, i);
  }
  return 0;
}

int getRegisterAsUnsignedByteArray(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "getRegisterAsUnsignedByteArray", "registers:getRegisterAsUnsignedByteArray(\"r4\", 2, 6)");
  return pushByteArrayFromAddressHelperFunction(luaState, true, "getRegisterAsUnsignedByteArray()");
}

int getRegisterAsSignedByteArray(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "getRegisterAsSignedByteArray", "registers:getRegisterAsSignedByteArray(\"r4\", 2, 6)");
  return pushByteArrayFromAddressHelperFunction(luaState, false, "getRegisterAsSignedByteArray()");
}

void writeValueToAddress(lua_State* luaState, u8* memoryLocation, NumberType valueType,
                         u8 registerSize, u8 offsetBytes)
{
  u8 valueTypeSize = getMaxSize(valueType);
  if (valueTypeSize > registerSize)
    luaL_error(luaState, "Error: in setRegister(), user tried to write a number that was larger than the register!");
  else if (valueTypeSize + offsetBytes > registerSize)
    luaL_error(luaState, "Error: in setRegister(), there was not enough room after offsetting the specified number of bytes in order to write the specified value.");
  

  u8 unsigned8 = 0;
  s8 signed8 = 0;
  u16 unsigned16 = 0;
  s16 signed16 = 0;
  u32 unsigned32 = 0;
  s32 signed32 = 0;
  float floatVal = 0.0;
  s64 signed64 = 0;
  double doubleVal = 0.0;

  switch (valueType)
  {
  case NumberType::UNSIGNED_8:
    unsigned8 = static_cast<u8>(luaL_checkinteger(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &unsigned8, 1);
    return;
  case NumberType::SIGNED_8:
    signed8 = static_cast<s8>(luaL_checkinteger(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &signed8, 1);
    return;
  case NumberType::UNSIGNED_16:
    unsigned16 = static_cast<u16>(luaL_checkinteger(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &unsigned16, 2);
    return;
  case NumberType::SIGNED_16:
    signed16 = static_cast<s16>(luaL_checkinteger(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &signed16, 2);
    return;
  case NumberType::UNSIGNED_32:
    unsigned32 = static_cast<u32>(luaL_checkinteger(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &unsigned32, 4);
    return;
  case NumberType::SIGNED_32:
    signed32 = static_cast<s32>(luaL_checkinteger(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &signed32, 4);
    return;
  case NumberType::UNSIGNED_64:
    luaL_error(luaState, "Error: UNSIGNED_64 is not a valid type for setRegister(), since Lua only "
                         "supports SIGNED_64 and DOUBLE internally. Please use S64 instead");
    return;
  case NumberType::SIGNED_64:
    signed64 = luaL_checkinteger(luaState, 4);
    memcpy(memoryLocation + offsetBytes, &signed64, 8);
    return;
  case NumberType::FLOAT:
    floatVal = static_cast<float>(luaL_checknumber(luaState, 4));
    memcpy(memoryLocation + offsetBytes, &floatVal, 4);
    return;
  case NumberType::DOUBLE:
    doubleVal = luaL_checknumber(luaState, 4);
    memcpy(memoryLocation + offsetBytes, &doubleVal, 8);
    return;
   default:
    luaL_error(luaState, "Error: Undefined type encountered in setRegister() function");
  }

}

int setRegister(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "setRegister", "registers:setRegister(\"r4\", \"u32\", 45, 4)");
  const char* registerString = luaL_checkstring(luaState, 2);
  RegisterObject registerObject = parseRegister(registerString);
  const char* typeString = luaL_checkstring(luaState, 3);
  NumberType valueType = parseType(typeString);
  if (valueType == NumberType::UNDEFINED)
    luaL_error(luaState, "Error: undefined type string was passed as an argument to setRegister()");
  u8 registerSize = 4;
  if (registerObject.regType == RegisterObject::REGISTER_TYPE::FLOATING_POINT_REGISTER)
    registerSize = 8;
  u8* address = getAddressForRegister(registerObject, luaState, "setRegister()");
  u8 offsetBytes = 0;
  if (lua_gettop(luaState) >= 5)
    offsetBytes = luaL_checkinteger(luaState, 5);
  if (offsetBytes < 0 || offsetBytes > 7)
    luaL_error(luaState, "Error: in setRegister(), number of bytes to offset from left was either less than 0 or more than 7");

  writeValueToAddress(luaState, address, valueType, registerSize, offsetBytes);

  return 0;
}

int setRegisterFromByteArray(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "setRegisterFromByteArray", "registers:setRegisterFromByteArray(\"r4\", byteTable, 2)");
  const char* registerString = luaL_checkstring(luaState, 2);
  RegisterObject registerObject = parseRegister(registerString);
  u8 registerSize = 4;
  if (registerObject.regType == RegisterObject::REGISTER_TYPE::FLOATING_POINT_REGISTER)
    registerSize = 8;
  s64 offsetBytes = 0;
  if (lua_gettop(luaState) >= 4)
    offsetBytes = luaL_checkinteger(luaState, 4);
  u8* address = getAddressForRegister(registerObject, luaState, "setRegisterFromByteArray()");

  u8 tempOffset = 0;
  s64 signed64 = 0;
  s8 signed8 = 0;
  u8 unsigned8 = 0;
  lua_pushnil(luaState);
  while (lua_next(luaState, 3) != 0)
  {
    if (tempOffset + offsetBytes > registerSize)
      luaL_error(luaState, "Error: in setRegisterFromByteArray(), attempt to write byte array past the end of the register occured!");
    signed64 = lua_tointeger(luaState, -1);
    if (signed64 < -128)
      luaL_error(luaState, "Error: in setRegisterFromByteArray(), attempted to write value from byte array which could not be represented with 1 byte (value was less than -128)");
    else if (signed64 > 255)
      luaL_error(luaState, "Error: in setRegisterFromByteArray(), attempted to write value from byte array which could not be represented with 1 byte (value was greater than 255)");
    if (signed64 < 0)
    {
      signed8 = static_cast<s8>(signed64);
      memcpy(address + offsetBytes + tempOffset, &signed8, 1);
    }
    else
    {
      unsigned8 = static_cast<u8>(signed64);
      memcpy(address + offsetBytes + tempOffset, &unsigned8, 1);
    }
    ++tempOffset;
    lua_pop(luaState, 1);
  }
  return 0;
}

}  // namespace LuaRegisters
}  // namespace Lua
