#include "LuaVersionResolver.h"
#include <unordered_map>

namespace Lua
{
bool isFirstVersionGreaterThanSecondVersion(const std::string& firstVersion,
                                            const std::string& secondVersion)
{
  return std::atof(firstVersion.c_str()) > std::atof(secondVersion.c_str());
}

bool isFirstVersionLessThanOrEqualToSecondVersion(const std::string& firstVersion,
                                                  const std::string& secondVersion)
{
  return std::atof(firstVersion.c_str()) <= std::atof(secondVersion.c_str());
}

std::vector<luaL_Reg> getLatestFunctionsForVersion(const luaL_Reg allFunctions[], size_t arraySize,
                                                   const std::string& LUA_API_VERSION)
{
  // This map contains key-value pairs of the format "functionName", {"functionName-VERSION-1.0",
  // functionLocation} For example, suppose we have a function that we want to be called
  // "writeBytes" in Lua scripts, which refers to a function called do_general_write on the backend.
  // The key value pairs might look like:
  //  "writeBytes", {"writeBytes-VERSION-1.3", do_general_write}
  std::unordered_map<std::string, luaL_Reg> functionToLatestVersionFoundMap =
      std::unordered_map<std::string, luaL_Reg>();

  for (int i = 0; i < arraySize; ++i)
  {
    std::string newFunctionNameWithVersion = allFunctions[i].name;
    size_t newLocationOfVersionString = newFunctionNameWithVersion.find("-VERSION-");

    std::string newCurrentVersionNumber =
        newFunctionNameWithVersion.substr(newLocationOfVersionString + 9);
    std::string newCurrentFunctionName =
        newFunctionNameWithVersion.substr(0, newLocationOfVersionString);
    if (functionToLatestVersionFoundMap.count(newCurrentFunctionName) == 0 &&
        !isFirstVersionGreaterThanSecondVersion(newCurrentVersionNumber, LUA_API_VERSION))
      functionToLatestVersionFoundMap[newCurrentFunctionName] = allFunctions[i];
    else
    {
      std::string functionAndVersionNameInMap =
          functionToLatestVersionFoundMap[newCurrentFunctionName].name;
      std::string versionNumberInMap =
          functionAndVersionNameInMap.substr(functionAndVersionNameInMap.find("-VERSION-") + 9);
      if (isFirstVersionGreaterThanSecondVersion(newCurrentVersionNumber, versionNumberInMap) &&
          !isFirstVersionGreaterThanSecondVersion(newCurrentVersionNumber, LUA_API_VERSION))
        functionToLatestVersionFoundMap[newCurrentFunctionName] = allFunctions[i];
    }
  }

  std::vector<luaL_Reg> finalListOfFunctionsForVersion = std::vector<luaL_Reg>();

  for (auto it = functionToLatestVersionFoundMap.begin();
       it != functionToLatestVersionFoundMap.end(); it++)
  {
    finalListOfFunctionsForVersion.push_back({it->first.c_str(), it->second.func});
  }

  finalListOfFunctionsForVersion.push_back({nullptr, nullptr});
  return finalListOfFunctionsForVersion;
}
}  // namespace Lua
