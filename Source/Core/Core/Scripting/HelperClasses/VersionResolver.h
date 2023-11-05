#include <string>
#include <unordered_map>
#include <vector>

#include <array>
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/VersionComparisonFunctions.h"

namespace Scripting
{
// Since these are template functions, their definition has to be in the header file.

template <size_t array_size>
std::vector<FunctionMetadata>
GetLatestFunctionsForVersion(const std::array<FunctionMetadata, array_size> all_functions,
                             const std::string& api_version,
                             std::unordered_map<std::string, std::string>&
                                 deprecated_functions_to_version_they_were_removed_in_map)
{
  // This map contains key-value pairs of the format "functionName", FunctionMetadata.
  //  For example, suppose we have a function that we want to be
  // called "writeBytes" in scripts, which refers to a function called do_general_write on the
  // backend. The key value pairs might look like:
  //  "writeBytes", {"writeBytes", "1.0", do_general_write, ScriptingEnums::ArgTypeEnum::VoidType,
  //  {ScriptingEnums::ArgTypeEnum::UnsignedByteVector}}
  std::unordered_map<std::string, FunctionMetadata> function_to_latest_version_found_map;

  for (int i = 0; i < array_size; ++i)
  {
    std::string current_function_name = all_functions[i].function_name;
    std::string function_version_number = all_functions[i].function_version;

    if (function_to_latest_version_found_map.count(current_function_name) == 0 &&
        !IsFirstVersionGreaterThanSecondVersion(function_version_number, api_version))
    {
      function_to_latest_version_found_map[current_function_name] = all_functions[i];
    }

    else if (function_to_latest_version_found_map.count(current_function_name) > 0 &&
             IsFirstVersionGreaterThanSecondVersion(
                 function_version_number,
                 function_to_latest_version_found_map[current_function_name].function_version) &&
             !IsFirstVersionGreaterThanSecondVersion(function_version_number, api_version))
    {
      function_to_latest_version_found_map[current_function_name] = all_functions[i];
    }
  }

  std::vector<FunctionMetadata> final_list_of_functions_for_version_with_deprecated_functions;

  for (auto it = function_to_latest_version_found_map.begin();
       it != function_to_latest_version_found_map.end(); it++)
  {
    final_list_of_functions_for_version_with_deprecated_functions.push_back(it->second);
  }

  std::vector<FunctionMetadata> final_list_of_functions_for_version;

  for (auto it = final_list_of_functions_for_version_with_deprecated_functions.begin();
       it != final_list_of_functions_for_version_with_deprecated_functions.end(); ++it)
  {
    if (!(deprecated_functions_to_version_they_were_removed_in_map.count(it->function_name) > 0 &&
          IsFirstVersionGreaterThanOrEqualToSecondVersion(
              api_version,
              deprecated_functions_to_version_they_were_removed_in_map[it->function_name])))
    {
      final_list_of_functions_for_version.push_back(*it);
    }
  }

  return final_list_of_functions_for_version;
}

template <size_t array_size>
FunctionMetadata GetFunctionForVersion(const std::array<FunctionMetadata, array_size> all_functions,
                                       const std::string& api_version,
                                       const std::string& function_name,
                                       std::unordered_map<std::string, std::string>&
                                           deprecated_functions_to_version_they_were_removed_in_map)
{
  std::vector<FunctionMetadata> functions_for_version = GetLatestFunctionsForVersion(
      all_functions, api_version, deprecated_functions_to_version_they_were_removed_in_map);

  for (auto& func : functions_for_version)
  {
    if (func.function_name == function_name)
      return func;
  }
  return {};
}

template <size_t array_size>
std::vector<FunctionMetadata>
GetAllFunctions(const std::array<FunctionMetadata, array_size> all_functions)
{
  return std::vector<FunctionMetadata>(&all_functions[0], &all_functions[0] + array_size);
}
}  // namespace Scripting
