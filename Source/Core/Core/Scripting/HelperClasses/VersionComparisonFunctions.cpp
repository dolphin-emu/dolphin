#include "Core/Scripting/HelperClasses/VersionComparisonFunctions.h"

namespace Scripting
{

// This helper function returns -1 if firstVersion < secondVersion, 0 if firstVersion ==
// secondVersion, and 1 if firstVersion > secondVersion
int CompareFirstVersionToSecondVersion(std::string first_version, std::string second_version)
{
  size_t number_of_periods_in_first_version =
      std::count(first_version.begin(), first_version.end(), '.');
  size_t number_of_periods_in_second_version =
      std::count(second_version.begin(), second_version.end(), '.');

  if (first_version[0] == '.')
    first_version = "0" + first_version;

  if (second_version[0] == '.')
    second_version = "0" + second_version;

  while (number_of_periods_in_first_version < number_of_periods_in_second_version)
  {
    first_version += ".0";
    ++number_of_periods_in_first_version;
  }

  while (number_of_periods_in_second_version < number_of_periods_in_first_version)
  {
    second_version += ".0";
    ++number_of_periods_in_second_version;
  }

  size_t index_of_next_digit_in_first_version = 0;
  size_t index_of_next_digit_in_second_version = 0;

  while (index_of_next_digit_in_first_version < first_version.length())
  {
    std::string next_number_in_first_version;
    std::string next_number_in_second_version;

    size_t index_of_next_period_in_first_version =
        first_version.find('.', index_of_next_digit_in_first_version);
    if (index_of_next_period_in_first_version == std::string::npos)
    {
      next_number_in_first_version = first_version.substr(index_of_next_digit_in_first_version);
      index_of_next_digit_in_first_version = first_version.length();
    }
    else
    {
      next_number_in_first_version = first_version.substr(index_of_next_digit_in_first_version,
                                                          index_of_next_period_in_first_version);
      index_of_next_digit_in_first_version = index_of_next_period_in_first_version + 1;
    }

    size_t index_of_next_period_in_second_version =
        second_version.find('.', index_of_next_digit_in_second_version);
    if (index_of_next_period_in_second_version == std::string::npos)
    {
      next_number_in_second_version = second_version.substr(index_of_next_digit_in_second_version);
      index_of_next_digit_in_second_version = second_version.length();
    }
    else
    {
      next_number_in_second_version = second_version.substr(index_of_next_digit_in_second_version,
                                                            index_of_next_period_in_second_version);
      index_of_next_digit_in_second_version = index_of_next_period_in_second_version + 1;
    }

    int first_number = std::stoi(next_number_in_first_version);
    int second_number = std::stoi(next_number_in_second_version);

    if (first_number > second_number)
      return 1;
    else if (first_number < second_number)
      return -1;
  }

  return 0;
}

bool IsFirstVersionGreaterThanSecondVersion(const std::string& first_version,
                                            const std::string& second_version)
{
  return CompareFirstVersionToSecondVersion(first_version, second_version) > 0;
}

bool IsFirstVersionGreaterThanOrEqualToSecondVersion(const std::string& first_version,
                                                     const std::string& second_version)
{
  return CompareFirstVersionToSecondVersion(first_version, second_version) >= 0;
}

}  // namespace Scripting
