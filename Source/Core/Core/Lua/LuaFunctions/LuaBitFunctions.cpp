#include "LuaBitFunctions.h"
#include "common/CommonTypes.h"

namespace Lua
{
namespace LuaBit
{

BitClass* GetBitInstance()
{
  if (bitInstance == NULL)
    bitInstance = new BitClass();
  return bitInstance;
}

  void InitLuaBitFunctions(lua_State* luaState)
  {
    BitClass** bitPtrPtr = (BitClass**)lua_newuserdata(luaState, sizeof(BitClass*));
    *bitPtrPtr = GetBitInstance();
    luaL_newmetatable(luaState, "LuaBitMetaTable");
    lua_pushvalue(luaState, -1);
    lua_setfield(luaState, -2, "__index");

    luaL_Reg luaBitFunctions[] = {
      {"bitwise_and", bitwise_and},
      {"bitwise_or", bitwise_or},
      {"bitwise_not", bitwise_not},
      {"bitwise_xor", bitwise_xor},
      {"logical_and", logical_and},
      {"logical_or", logical_or},
      {"logical_not", logical_not},
      {"bit_shift_left", bit_shift_left},
      {"bit_shift_right", bit_shift_right},
      {nullptr, nullptr}};

    luaL_setfuncs(luaState, luaBitFunctions, 0);
    lua_setmetatable(luaState, -2);
    lua_setglobal(luaState, "bit");
  }

int bitwise_and(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal & secondVal);
  return 1;
}

int bitwise_or(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal | secondVal);
  return 1;
}

int bitwise_not(lua_State* luaState)
{
  u32 inputVal = luaL_checknumber(luaState, 2);
  lua_pushinteger(luaState, (u32) ~inputVal);
  return 1;
}

int bitwise_xor(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal ^ secondVal);
  return 1;
}

int logical_and(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal && secondVal);
  return 1;
}

int logical_or(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal || secondVal);
  return 1;
}

int logical_not(lua_State* luaState)
{
  u32 inputVal = luaL_checknumber(luaState, 2);
  lua_pushinteger(luaState, !inputVal);
  return 1;
}

int bit_shift_left(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal << secondVal);
  return 1;
}

int bit_shift_right(lua_State* luaState)
{
  u32 firstVal = luaL_checknumber(luaState, 2);
  u32 secondVal = luaL_checknumber(luaState, 3);
  lua_pushinteger(luaState, firstVal >> secondVal);
  return 1;
}
}
}
