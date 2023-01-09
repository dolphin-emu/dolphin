#include "LuaRegisters.h"
#include "NumberType.h"
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
    {"setRegister",  setRegister},
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

void pushValueFromAddress(lua_State* luaState, u8* memoryLocation, NumberType returnType, u8 maxSize, u8 offsetBytes)
{
  u8 returnTypeSize = getMaxSize(returnType);
  if (returnTypeSize > maxSize)
    luaL_error(luaState, "Error: in getRegister(), user requested a return type that was 64 bits on a register which is only 32 bits long!");
  else if (returnTypeSize + offsetBytes > maxSize)
    luaL_error(luaState, "Error: in getRegister(), there was not enough room after offsetting the specified number of bytes in order to ge the specified data type.");

  u8 unsigned8 = 0;
  s8 signed8 = 0;
  u16 unsigned16 = 0;
  s16 signed16 = 0;
  u32 unsigned32 = 0;
  s32 signed32 = 0;
  float floatVal = 0.0;
  u64 unsigned64 = 0;
  s64 signed64 = 0;
  double doubleVal = 0.0;

  switch (returnType)
  {
  case NumberType::UNSIGNED_8:
    memcpy(&unsigned8, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, unsigned8);
    break;
  case NumberType::SIGNED_8:
    memcpy(&signed8, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed8);
    break;
  case NumberType::UNSIGNED_16:
    memcpy(&unsigned16, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, unsigned16);
    break;
  case NumberType::SIGNED_16:
    memcpy(&signed16, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed16);
    break;
  case NumberType::UNSIGNED_32:
    memcpy(&unsigned32, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, unsigned32);
    break;
  case NumberType::SIGNED_32:
    memcpy(&signed32, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed32);
    break;
  case NumberType::FLOAT:
    memcpy(&floatVal, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushnumber(luaState, floatVal);
    break;
  case NumberType::UNSIGNED_64:
    memcpy(&unsigned64, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushnumber(luaState, static_cast<double>(unsigned64)); // this casting is done to prevent a Lua Overflow Error
    break;
  case NumberType::SIGNED_64:
    memcpy(&signed64, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushinteger(luaState, signed64);
    break;
  case NumberType::DOUBLE:
    memcpy(&doubleVal, memoryLocation + offsetBytes, returnTypeSize);
    lua_pushnumber(luaState, doubleVal);
    break;
  default:
    luaL_error(luaState, "Error: Unknown type string passed in to getRegister() function");
  }
}

int getRegister(lua_State* luaState)
{
  const char* returnTypeString = luaL_checkstring(luaState, 2);
  NumberType returnType = parseType(returnTypeString);
  if (returnType == NumberType::UNDEFINED)
    luaL_error(luaState, "Error: undefined type string was passed as an argument to getRegister()");
  const char* registerString = luaL_checkstring(luaState, 3);
  RegisterObject registerObject = parseRegister(registerString);
  u8 regNum = 0;
  s64 offsetBytes = 0;
  if (lua_gettop(luaState) >= 4)
    offsetBytes = luaL_checkinteger(luaState, 4);
  if (offsetBytes < 0 || offsetBytes > 8)
    luaL_error(luaState, "Error: in getRegister(), number of bytes to offset from left was either less than 0 or more than 8");
  u8* address = NULL;
  switch (registerObject.regType)
  {
  case RegisterObject::REGISTER_TYPE::GENERAL_PURPOSE_REGISTER :
    regNum = registerObject.registerNum;
    address = ((u8*) (rGPR)) + regNum;
    pushValueFromAddress(luaState, address, returnType, 4, offsetBytes);
    break;
  case RegisterObject::REGISTER_TYPE::PC_REGISTER:
    address = ((u8*) &PC);
    pushValueFromAddress(luaState, address, returnType, 4, offsetBytes);
    break;
  case RegisterObject::REGISTER_TYPE::RETURN_REGISTER:
    address = ((u8*) &LR);
    pushValueFromAddress(luaState, address, returnType, 4, offsetBytes);
    break;
  case RegisterObject::REGISTER_TYPE::FLOATING_POINT_REGISTER:
    address = ((u8*)PowerPC::ppcState.ps) + regNum;
    pushValueFromAddress(luaState, address, returnType, 8, offsetBytes);
    break;

  default:
    luaL_error(luaState,
               "Error: Invalid string passed in for register name in getRegister(). Currently, "
               "R0-R31, F0-F31, PC, and LR are the only registers which are supported");
  }
  return 1;
}

int setRegister(lua_State* luaState)
{
  return 0;
}

}  // namespace LuaRegisters
}  // namespace Lua
