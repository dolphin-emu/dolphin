#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"

#include <fmt/format.h>
#include <memory>
#include "Common/CommonTypes.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::BitApi
{

const char* class_name = "bit";
static std::array all_bit_functions_metadata_list = {
      FunctionMetadata("bitwise_and", "1.0", "bitwise_and(17, 81)", BitwiseAnd, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("bitwise_or", "1.0", "bitwise_or(19, 31)", BitwiseOr, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("bitwise_not", "1.0", "bitwise_not(41)", BitwiseNot, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong}),
      FunctionMetadata("bitwise_xor", "1.0", "bitwise_xor(21, 40)", BitwiseXor, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("logical_and", "1.0", "logical_and(true, false)", LogicalAnd, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("logical_or", "1.0", "logical_or(true, false)", LogicalOr, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("logical_xor", "1.0", "logical_xor(true, false)", LogicalXor, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("logical_not", "1.0", "logical_not(true)", LogicalNot, ArgTypeEnum::Boolean,
                       {ArgTypeEnum::LongLong}),
      FunctionMetadata("bit_shift_left", "1.0", "bit_shift_left(3, 6)", BitShiftLeft, ArgTypeEnum::LongLong,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::LongLong}),
      FunctionMetadata("bit_shift_right", "1.0", "bit_shift_right(100, 2)", BitShiftRight, ArgTypeEnum::LongLong,
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
  if (first_val < 0)
    return CreateErrorStringArgHolder("first argument passed to the function was negative. Both "
                                      "arguments to the function must be positive!");
  else if (second_val < 0)
    return CreateErrorStringArgHolder("second argument passed to the function was negative. Both "
                                      "arguments to the function must be positive!");

  return CreateLongLongArgHolder(
      static_cast<s64>(static_cast<u64>(first_val) << static_cast<u64>(second_val)));
}

ArgHolder BitShiftRight(std::vector<ArgHolder>& args_list)
{
  s64 first_val = args_list[0].long_long_val;
  s64 second_val = args_list[1].long_long_val;

  if (first_val < 0)
    return CreateErrorStringArgHolder("first argument passed to the function was negative. Both "
                                      "arguments to the function must be positive!");
  else if (second_val < 0)
    return CreateErrorStringArgHolder("second argument passed to the function was negative. Both "
                                      "arguments to the function must be positive!");

  return CreateLongLongArgHolder(
      static_cast<s64>(static_cast<u64>(first_val) >> static_cast<u64>(second_val)));
}
}  // namespace Scripting::BitApi
