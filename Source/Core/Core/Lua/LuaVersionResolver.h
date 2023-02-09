#include <vector>
#include <string>
#include <unordered_map>

#include <lua.hpp>

#include <array>
#include "LuaHelperClasses/LuaL_Reg_With_Version.h"
#include "LuaHelperClasses/LuaVersionComparisonFunctions.h"

namespace Lua
{

  template <typename size_t arraySize>
  void addLatestFunctionsForVersion(const std::array<luaL_Reg_With_Version, arraySize> allFunctions, const std::string& LUA_API_VERSION, std::unordered_map<std::string, std::string>& deprecatedFunctionsToVersionTheyWereRemovedInMap, lua_State* luaState)
{
  // This map contains key-value pairs of the format "functionName", {"functionName",
  // "versionNumber", functionLocation} For example, suppose we have a function that we want to be
  // called "writeBytes" in Lua scripts, which refers to a function called do_general_write on the
  // backend. The key value pairs might look like:
  //  "writeBytes", {"writeBytes", "1.3", do_general_write}
  std::unordered_map<std::string, luaL_Reg_With_Version> functionToLatestVersionFoundMap = std::unordered_map<std::string, luaL_Reg_With_Version>();

  for (int i = 0; i < arraySize; ++i)
  {
    std::string currentFunctionName = allFunctions[i].name;
    std::string functionVersionNumber = allFunctions[i].version;
    
    if (functionToLatestVersionFoundMap.count(currentFunctionName) == 0 && !isFirstVersionGreaterThanSecondVersion(functionVersionNumber, LUA_API_VERSION))
      functionToLatestVersionFoundMap[currentFunctionName] = allFunctions[i];

    else if (isFirstVersionGreaterThanSecondVersion(functionVersionNumber, functionToLatestVersionFoundMap[currentFunctionName].version) && !isFirstVersionGreaterThanSecondVersion(functionVersionNumber, LUA_API_VERSION))
      functionToLatestVersionFoundMap[currentFunctionName] = allFunctions[i];
  }
  
  std::vector<luaL_Reg_With_Version> finalListOfFunctionsForVersionWithDeprecatedFunctions = std::vector<luaL_Reg_With_Version>();

  for (auto it = functionToLatestVersionFoundMap.begin(); it != functionToLatestVersionFoundMap.end(); it++)
  {
    finalListOfFunctionsForVersionWithDeprecatedFunctions.push_back(it->second);
  }

  std::vector<luaL_Reg> finalListOfFunctionsForVersion = std::vector<luaL_Reg>();

  for (auto it = finalListOfFunctionsForVersionWithDeprecatedFunctions.begin(); it != finalListOfFunctionsForVersionWithDeprecatedFunctions.end(); ++it)
  {
    if (!(deprecatedFunctionsToVersionTheyWereRemovedInMap.count(it->name) > 0 && isFirstVersionGreaterThanOrEqualToSecondVersion(LUA_API_VERSION, deprecatedFunctionsToVersionTheyWereRemovedInMap[it->name])))
    {
      finalListOfFunctionsForVersion.push_back({it->name, it->func});
    }
  }

  finalListOfFunctionsForVersion.push_back({nullptr, nullptr});

  luaL_setfuncs(luaState, &finalListOfFunctionsForVersion[0], 0);
  lua_setmetatable(luaState, -2);
}
}  // namespace Lua
