#include "LuaVersionComparisonFunctions.h"
namespace Lua
{
// This helper function returns -1 if firstVersion < secondVersion, 0 if firstVersion ==
// secondVersion, and 1 if firstVersion > secondVersion
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

}
