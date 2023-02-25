#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"

#include <fmt/format.h>
#include <memory>
#include "Common/CommonTypes.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::BitApi
{

  const char* class_name = "bit";
static std::array all_bit_functions_metadata_list = {
    FunctionMetadata("bitwise_and", "1.0", BitwiseAnd, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("bitwise_or", "1.0", BitwiseOr, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("bitwise_not", "1.0", BitwiseNot, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong}),
    FunctionMetadata("bitwise_xor", "1.0", BitwiseXor, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("logical_and", "1.0", LogicalAnd, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("logical_or", "1.0", LogicalOr, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("logical_xor", "1.0", LogicalXor, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("logical_not", "1.0", LogicalNot, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("bit_shift_left", "1.0", BitShiftLeft, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
    FunctionMetadata("bit_shift_right", "1.0", BitShiftRight, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
};

 ClassMetadata GetBitApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_bit_functions_metadata_list, api_version, deprecated_functions_map)};
}

ArgHolder BitwiseAnd(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  return CreateLongLongArgHolder(first_val & second_val);
}

ArgHolder BitwiseOr(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  return CreateLongLongArgHolder(first_val | second_val);
}

ArgHolder BitwiseNot(std::vector<ArgHolder>& args_list)
{
  s64 input_val = args_list[0].long_long_val;
  return CreateLongLongArgHolder(~input_val);
}

ArgHolder BitwiseXor(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  return CreateLongLongArgHolder(first_val ^ second_val);
}

ArgHolder LogicalAnd(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  return CreateBoolArgHolder(first_val && second_val);
}

ArgHolder LogicalOr(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  return CreateBoolArgHolder(first_val || second_val);
}

ArgHolder LogicalXor(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  return CreateBoolArgHolder((first_val || second_val) && !(first_val && second_val));
}

ArgHolder LogicalNot(std::vector<ArgHolder>& args_list)
{
  s64 input_val = args_list[0].long_long_val;
  return CreateBoolArgHolder(!input_val);
}

ArgHolder BitShiftLeft(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  /* if (first_val < 0 || second_val < 0)
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}() function, an argument passed into the function was "
                           "negative. Both arguments to the function must be positive!",
                           class_name, "bit_shift_left")
                   .c_str());*/
  return CreateLongLongArgHolder(
      static_cast<s64>(static_cast<u64>(first_val) << static_cast<u64>(second_val)));
}

ArgHolder BitShiftRight(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;
  /* if (first_val < 0 || second_val < 0)
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}() function, an argument passed into the function was "
                           "negative. Both arguments to the function must be positive!",
                           class_name, "bit_shift_right")
                   .c_str());*/
  return CreateLongLongArgHolder(
      static_cast<s64>(static_cast<u64>(first_val) >> static_cast<u64>(second_val)));
}
}  // namespace Scripting::BitApi
