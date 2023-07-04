#include "Core/Scripting/InternalAPIModules/BitAPI.h"

#include <fmt/format.h>
#include <memory>
#include "Common/CommonTypes.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::BitApi
{

const char* class_name = "BitAPI";
static std::array all_bit_functions_metadata_list = {
    FunctionMetadata("bitwise_and", "1.0", "bitwise_and(17, 81)", BitwiseAnd,
                     ScriptingEnums::ArgTypeEnum::S64,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("bitwise_or", "1.0", "bitwise_or(19, 31)", BitwiseOr,
                     ScriptingEnums::ArgTypeEnum::S64,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("bitwise_not", "1.0", "bitwise_not(41)", BitwiseNot,
                     ScriptingEnums::ArgTypeEnum::S64, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("bitwise_xor", "1.0", "bitwise_xor(21, 40)", BitwiseXor,
                     ScriptingEnums::ArgTypeEnum::S64,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("logical_and", "1.0", "logical_and(true, false)", LogicalAnd,
                     ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("logical_or", "1.0", "logical_or(true, false)", LogicalOr,
                     ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("logical_xor", "1.0", "logical_xor(true, false)", LogicalXor,
                     ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("logical_not", "1.0", "logical_not(true)", LogicalNot,
                     ScriptingEnums::ArgTypeEnum::Boolean, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("bit_shift_left", "1.0", "bit_shift_left(3, 6)", BitShiftLeft,
                     ScriptingEnums::ArgTypeEnum::S64,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("bit_shift_right", "1.0", "bit_shift_right(100, 2)", BitShiftRight,
                     ScriptingEnums::ArgTypeEnum::S64,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),
};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_bit_functions_metadata_list, api_version,
                                                   deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_bit_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_bit_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder* BitwiseAnd(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  return CreateS64ArgHolder(first_val & second_val);
}

ArgHolder* BitwiseOr(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  return CreateS64ArgHolder(first_val | second_val);
}

ArgHolder* BitwiseNot(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 input_val = (*args_list)[0]->s64_val;
  return CreateS64ArgHolder(~input_val);
}

ArgHolder* BitwiseXor(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  return CreateS64ArgHolder(first_val ^ second_val);
}

ArgHolder* LogicalAnd(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  return CreateBoolArgHolder(first_val && second_val);
}

ArgHolder* LogicalOr(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  return CreateBoolArgHolder(first_val || second_val);
}

ArgHolder* LogicalXor(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  return CreateBoolArgHolder((first_val || second_val) && !(first_val && second_val));
}

ArgHolder* LogicalNot(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 input_val = (*args_list)[0]->s64_val;
  return CreateBoolArgHolder(!input_val);
}

ArgHolder* BitShiftLeft(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;
  if (first_val < 0)
    return CreateErrorStringArgHolder("first argument passed into the function was negative. Both "
                                      "arguments to the function must be positive!");
  else if (second_val < 0)
    return CreateErrorStringArgHolder("second argument passed into the function was negative. Both "
                                      "arguments to the function must be positive!");

  return CreateS64ArgHolder(
      static_cast<s64>(static_cast<u64>(first_val) << static_cast<u64>(second_val)));
}

ArgHolder* BitShiftRight(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  s64 first_val = (*args_list)[0]->s64_val;
  s64 second_val = (*args_list)[1]->s64_val;

  if (first_val < 0)
    return CreateErrorStringArgHolder("first argument passed to the function was negative. Both "
                                      "arguments to the function must be positive!");
  else if (second_val < 0)
    return CreateErrorStringArgHolder("second argument passed to the function was negative. Both "
                                      "arguments to the function must be positive!");

  return CreateS64ArgHolder(
      static_cast<s64>(static_cast<u64>(first_val) >> static_cast<u64>(second_val)));
}
}  // namespace Scripting::BitApi
