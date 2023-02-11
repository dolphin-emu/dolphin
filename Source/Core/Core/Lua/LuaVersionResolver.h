#include <string>
#include <unordered_map>
#include <vector>

#include <lua.hpp>

#include <array>
#include "LuaHelperClasses/luaL_Reg_With_Version.h"
#include "LuaHelperClasses/LuaVersionComparisonFunctions.h"

namespace Lua
{

template <size_t array_size>
void AddLatestFunctionsForVersion(const std::array<luaL_Reg_With_Version, array_size> all_functions,
                                  const std::string& lua_api_version,
                                  std::unordered_map<std::string, std::string>&
                                      deprecated_functions_to_version_they_were_removed_in_map,
                                  lua_State* lua_state)
{
  // This map contains key-value pairs of the format "functionName", {"functionName",
  // "versionNumber", functionLocation} For example, suppose we have a function that we want to be
  // called "writeBytes" in Lua scripts, which refers to a function called do_general_write on the
  // backend. The key value pairs might look like:
  //  "writeBytes", {"writeBytes", "1.3", do_general_write}
  std::unordered_map<std::string, luaL_Reg_With_Version> function_to_latest_version_found_map;

  for (int i = 0; i < array_size; ++i)
  {
    std::string current_function_name = all_functions[i].name;
    std::string function_version_number = all_functions[i].version;

    if (function_to_latest_version_found_map.count(current_function_name) == 0 &&
        !IsFirstVersionGreaterThanSecondVersion(function_version_number, lua_api_version))
      function_to_latest_version_found_map[current_function_name] = all_functions[i];

    else if (IsFirstVersionGreaterThanSecondVersion(
                 function_version_number,
                 function_to_latest_version_found_map[current_function_name].version) &&
             !IsFirstVersionGreaterThanSecondVersion(function_version_number, lua_api_version))
      function_to_latest_version_found_map[current_function_name] = all_functions[i];
  }

  std::vector<luaL_Reg_With_Version> final_list_of_functions_for_version_with_deprecated_functions;

  for (auto it = function_to_latest_version_found_map.begin();
       it != function_to_latest_version_found_map.end(); it++)
  {
    final_list_of_functions_for_version_with_deprecated_functions.push_back(it->second);
  }

  std::vector<luaL_Reg> final_list_of_functions_for_version;

  for (auto it = final_list_of_functions_for_version_with_deprecated_functions.begin();
       it != final_list_of_functions_for_version_with_deprecated_functions.end(); ++it)
  {
    if (!(deprecated_functions_to_version_they_were_removed_in_map.count(it->name) > 0 &&
          IsFirstVersionGreaterThanOrEqualToSecondVersion(
              lua_api_version, deprecated_functions_to_version_they_were_removed_in_map[it->name])))
    {
      final_list_of_functions_for_version.push_back({it->name, it->func});
    }
  }

  final_list_of_functions_for_version.push_back({nullptr, nullptr});

  luaL_setfuncs(lua_state, &final_list_of_functions_for_version[0], 0);
  lua_setmetatable(lua_state, -2);
}
}  // namespace Lua
