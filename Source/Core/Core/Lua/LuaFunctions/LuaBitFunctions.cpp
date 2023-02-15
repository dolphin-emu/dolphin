#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"

#include <fmt/format.h>
#include <memory>
#include "Common/CommonTypes.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaBit
{
const char* class_name = "bitAPI";

class BitClass
{
public:
  BitClass() {}
};

static std::unique_ptr<BitClass> bit_instance = nullptr;

BitClass* GetBitInstance()
{
  if (bit_instance == nullptr)
    bit_instance = std::make_unique<BitClass>(BitClass());
  return bit_instance.get();
}

void InitLuaBitFunctions(lua_State* lua_state, const std::string& lua_api_version)
{
  BitClass** bit_ptr_ptr = (BitClass**)lua_newuserdata(lua_state, sizeof(BitClass*));
  *bit_ptr_ptr = GetBitInstance();
  luaL_newmetatable(lua_state, "LuaBitMetaTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_bit_functions_with_versions_attached = {
      luaL_Reg_With_Version({"bitwise_and", "1.0", BitwiseAnd}),
      luaL_Reg_With_Version({"bitwise_or", "1.0", BitwiseOr}),
      luaL_Reg_With_Version({"bitwise_not", "1.0", BitwiseNot}),
      luaL_Reg_With_Version({"bitwise_xor", "1.0", BitwiseXor}),
      luaL_Reg_With_Version({"logical_and", "1.0", LogicalAnd}),
      luaL_Reg_With_Version({"logical_or", "1.0", LogicalOr}),
      luaL_Reg_With_Version({"logical_xor", "1.0", LogicalXor}),
      luaL_Reg_With_Version({"logical_not", "1.0", LogicalNot}),
      luaL_Reg_With_Version({"bit_shift_left", "1.0", BitShiftLeft}),
      luaL_Reg_With_Version({"bit_shift_right", "1.0", BitShiftRight})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_bit_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, class_name);
}

int BitwiseAnd(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "bitwise_and",
                            "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  lua_pushinteger(lua_state, first_val & second_val);
  return 1;
}

int BitwiseOr(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "bitwise_or", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  lua_pushinteger(lua_state, first_val | second_val);
  return 1;
}

int BitwiseNot(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "bitwise_not", "(exampleInteger)");
  s64 input_val = luaL_checkinteger(lua_state, 2);
  lua_pushinteger(lua_state, ~input_val);
  return 1;
}

int BitwiseXor(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "bitwise_xor", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  lua_pushinteger(lua_state, first_val ^ second_val);
  return 1;
}

int LogicalAnd(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "logical_and", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  lua_pushboolean(lua_state, first_val && second_val);
  return 1;
}

int LogicalOr(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "logical_or", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  lua_pushboolean(lua_state, first_val || second_val);
  return 1;
}

int LogicalXor(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "logical_xor", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  lua_pushboolean(lua_state, (first_val || second_val) && !(first_val && second_val));
  return 1;
}

int LogicalNot(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "logical_not", "(exampleInteger)");
  s64 input_val = luaL_checkinteger(lua_state, 2);
  lua_pushboolean(lua_state, !input_val);
  return 1;
}

int BitShiftLeft(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "bit_shit_left", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  if (first_val < 0 || second_val < 0)
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}() function, an argument passed into the function was "
               "negative. Both arguments to the function must be positive!", class_name, "bit_shift_left")
               .c_str());
  lua_pushinteger(lua_state,
                  static_cast<s64>(static_cast<u64>(first_val) << static_cast<u64>(second_val)));
  return 1;
}

int BitShiftRight(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "bit_shift_right", "(integer1, integer2)");
  s64 first_val = luaL_checkinteger(lua_state, 2);
  s64 second_val = luaL_checkinteger(lua_state, 3);
  if (first_val < 0 || second_val < 0)
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}() function, an argument passed into the function was "
               "negative. Both arguments to the function must be positive!", class_name, "bit_shift_right")
               .c_str());
  lua_pushinteger(lua_state,
                  static_cast<s64>(static_cast<u64>(first_val) >> static_cast<u64>(second_val)));
  return 1;
}
}  // namespace Lua::LuaBit
