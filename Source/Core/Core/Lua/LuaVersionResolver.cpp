#include "LuaVersionResolver.h"

namespace Lua
{

  //This helper function returns -1 if firstVersion < secondVersion, 0 if firstVersion == secondVersion, and 1 if firstVersion > secondVersion
int compareFirstVersionToSecondVersion(std::string firstVersion, std::string secondVersion)
{
  size_t numberOfPeriodsInFirstVersion = std::count(firstVersion.begin(), firstVersion.end(), '.');
  size_t numberOfPeriodsInSecondVersion = std::count(secondVersion.begin(), secondVersion.end(), '.');

  if (firstVersion[0] == '.')
    firstVersion = "0" + firstVersion;

  if (secondVersion[0] == '.')
    secondVersion = "0" + secondVersion;

  while (numberOfPeriodsInFirstVersion < numberOfPeriodsInSecondVersion)
  {
    firstVersion += ".0";
    ++numberOfPeriodsInFirstVersion;
  }

  while (numberOfPeriodsInSecondVersion < numberOfPeriodsInFirstVersion)
  {
    secondVersion += ".0";
    ++numberOfPeriodsInSecondVersion;
  }

  size_t indexOfNextDigitInFirstVersion = 0;
  size_t indexOfNextDigitInSecondVersion = 0;

  while (indexOfNextDigitInFirstVersion < firstVersion.length())
  {
    std::string nextNumberInFirstVersion;
    std::string nextNumberInSecondVersion;

    size_t indexOfNextPeriodInFirstVersion = firstVersion.find('.', indexOfNextDigitInFirstVersion);
    if (indexOfNextPeriodInFirstVersion == std::string::npos)
    {
      nextNumberInFirstVersion = firstVersion.substr(indexOfNextDigitInFirstVersion);
      indexOfNextDigitInFirstVersion = firstVersion.length();
    }
    else
    {
      nextNumberInFirstVersion = firstVersion.substr(indexOfNextDigitInFirstVersion, indexOfNextPeriodInFirstVersion);
      indexOfNextDigitInFirstVersion = indexOfNextPeriodInFirstVersion + 1;
    }

    size_t indexOfNextPeriodInSecondVersion = secondVersion.find('.', indexOfNextDigitInSecondVersion);
    if (indexOfNextPeriodInSecondVersion == std::string::npos)
    {
      nextNumberInSecondVersion = secondVersion.substr(indexOfNextDigitInSecondVersion);
      indexOfNextDigitInSecondVersion = secondVersion.length();
    }
    else
    {
      nextNumberInSecondVersion = secondVersion.substr(indexOfNextDigitInSecondVersion, indexOfNextPeriodInSecondVersion);
      indexOfNextDigitInSecondVersion = indexOfNextPeriodInSecondVersion + 1;
    }

    int firstNumber = std::stoi(nextNumberInFirstVersion);
    int secondNumber = std::stoi(nextNumberInSecondVersion);

    if (firstNumber > secondNumber)
      return 1;
    else if (firstNumber < secondNumber)
      return -1;

  }

  return 0;
}

bool isFirstVersionGreaterThanSecondVersion(const std::string& firstVersion, const std::string& secondVersion)
{
  return compareFirstVersionToSecondVersion(firstVersion, secondVersion) > 0;
}

bool isFirstVersionGreaterThanOrEqualToSecondVersion(const std::string& firstVersion, const std::string& secondVersion)
{
  return compareFirstVersionToSecondVersion(firstVersion, secondVersion) >= 0;
}

std::vector<luaL_Reg> getLatestFunctionsForVersion(const luaL_Reg_With_Version allFunctions[], size_t arraySize, const std::string& LUA_API_VERSION, std::unordered_map<std::string, std::string>& deprecatedFunctionsToVersionTheyWereRemovedInMap)
{
  // This map contains key-value pairs of the format "functionName", {"functionName-VERSION-1.0",
  // functionLocation} For example, suppose we have a function that we want to be called
  // "writeBytes" in Lua scripts, which refers to a function called do_general_write on the backend.
  // The key value pairs might look like:
  //  "writeBytes", {"writeBytes-VERSION-1.3", do_general_write}
  std::unordered_map<std::string, luaL_Reg_With_Version> functionToLatestVersionFoundMap =
      std::unordered_map<std::string, luaL_Reg_With_Version>();

  for (int i = 0; i < arraySize; ++i)
  {
    if (functionToLatestVersionFoundMap.count(allFunctions[i].name) == 0 &&
        !isFirstVersionGreaterThanSecondVersion(allFunctions[i].version, LUA_API_VERSION))
      functionToLatestVersionFoundMap[allFunctions[i].name] = allFunctions[i];

    else if (isFirstVersionGreaterThanSecondVersion(allFunctions[i].version, functionToLatestVersionFoundMap[allFunctions[i].name].version) &&
          !isFirstVersionGreaterThanSecondVersion(allFunctions[i].version, LUA_API_VERSION))
        functionToLatestVersionFoundMap[allFunctions[i].name] = allFunctions[i];
    }

  std::vector<luaL_Reg_With_Version> finalListOfFunctionsForVersionWithDeprecatedFunctions = std::vector<luaL_Reg_With_Version>();

  for (auto it = functionToLatestVersionFoundMap.begin(); it != functionToLatestVersionFoundMap.end(); it++)
  {
    finalListOfFunctionsForVersionWithDeprecatedFunctions.push_back({it->first.c_str(), it->second.version, it->second.func});
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
  return finalListOfFunctionsForVersion;
}
}  // namespace Lua
