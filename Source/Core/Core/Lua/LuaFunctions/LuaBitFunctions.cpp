#include "LuaBitFunctions.h"
#include "../LuaVersionResolver.h"
#include "common/CommonTypes.h"

namespace Lua
{
namespace LuaBit
{
class BitClass
{
public:
  BitClass() {}
};

BitClass* bitInstance;

BitClass* GetBitInstance()
{
  if (bitInstance == NULL)
    bitInstance = new BitClass();
  return bitInstance;
}

  void InitLuaBitFunctions(lua_State* luaState, const std::string& luaApiVersion)
  {
    BitClass** bitPtrPtr = (BitClass**)lua_newuserdata(luaState, sizeof(BitClass*));
    *bitPtrPtr = GetBitInstance();
    luaL_newmetatable(luaState, "LuaBitMetaTable");
    lua_pushvalue(luaState, -1);
    lua_setfield(luaState, -2, "__index");

    luaL_Reg_With_Version luaBitFunctionsWithVersionsAttached[] = {
      {"bitwise_and", "1.0", bitwise_and},
      {"bitwise_or", "1.0", bitwise_or},
      {"bitwise_not", "1.0", bitwise_not},
      {"bitwise_xor", "1.0", bitwise_xor},
      {"logical_and", "1.0", logical_and},
      {"logical_or", "1.0", logical_or},
      {"logical_xor", "1.0", logical_xor},
      {"logical_not", "1.0", logical_not},
      {"bit_shift_left", "1.0", bit_shift_left},
      {"bit_shift_right", "1.0", bit_shift_right}};

    std::unordered_map<std::string, std::string> deprecatedFunctionsMap = std::unordered_map<std::string, std::string>();
    std::vector<luaL_Reg> vectorOfFunctionsForVersion = getLatestFunctionsForVersion(luaBitFunctionsWithVersionsAttached, 10, luaApiVersion, deprecatedFunctionsMap);
    luaL_setfuncs(luaState, &vectorOfFunctionsForVersion[0], 0);
    lua_setmetatable(luaState, -2);
    lua_setglobal(luaState, "bit");
  }

int bitwise_and(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "bitwise_and", "bitwise_and(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  lua_pushinteger(luaState, firstVal & secondVal);
  return 1;
}

int bitwise_or(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "bitwise_or", "bitwise_or(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  lua_pushinteger(luaState, firstVal | secondVal);
  return 1;
}

int bitwise_not(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "bitwise_not", "bitwise_not(exampleInteger)");
  s64 inputVal = luaL_checkinteger(luaState, 2);
  lua_pushinteger(luaState, ~inputVal);
  return 1;
}

int bitwise_xor(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "bitwise_xor", "bitwise_xor(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  lua_pushinteger(luaState, firstVal ^ secondVal);
  return 1;
}

int logical_and(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "logical_and", "logical_and(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  lua_pushboolean(luaState, firstVal && secondVal);
  return 1;
}

int logical_or(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "logical_or", "logical_or(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  lua_pushboolean(luaState, firstVal || secondVal);
  return 1;
}

int logical_xor(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "logical_xor", "logical_xor(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  lua_pushboolean(luaState, (firstVal || secondVal) && !(firstVal && secondVal));
  return 1;

}

int logical_not(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "logical_not", "logical_not(exampleInteger)");
  s64 inputVal = luaL_checkinteger(luaState, 2);
  lua_pushboolean(luaState, !inputVal);
  return 1;
}

int bit_shift_left(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "bit_shit_left", "bit_shift_left(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  if (firstVal < 0 || secondVal < 0)
    luaL_error(luaState, "Error: in bit:bit_shift_left() function, an argument passed into the function was negative. Both arguments to the function must be positive!");
  lua_pushinteger(luaState, static_cast<s64>(static_cast<u64>(firstVal) << static_cast<u64>(secondVal)));
  return 1;
}

int bit_shift_right(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "bit_shift_right", "bit_shift_right(integer1, integer2)");
  s64 firstVal = luaL_checkinteger(luaState, 2);
  s64 secondVal = luaL_checkinteger(luaState, 3);
  if (firstVal < 0 || secondVal < 0)
    luaL_error(luaState, "Error: in bit:bit_shift_right() function, an argument passed into the function was negative. Both arguments to the function must be positive!");
  lua_pushinteger(luaState, static_cast<s64>(static_cast<u64>(firstVal) >> static_cast<u64>(secondVal)));
  return 1;
}
}
}
