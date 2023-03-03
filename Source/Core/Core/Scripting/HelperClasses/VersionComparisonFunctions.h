#ifndef VERSION_COMPARISON_FUNCTIONS
#define VERSION_COMPARISON_FUNCTIONS
#include <string>

namespace Scripting
{
int CompareFirstVersionToSecondVersion(std::string first_version, std::string second_version);
bool IsFirstVersionGreaterThanSecondVersion(const std::string& first_version,
                                            const std::string& second_version);
bool IsFirstVersionGreaterThanOrEqualToSecondVersion(const std::string& first_version,
                                                     const std::string& second_version);
}  // namespace Scripting
#endif
