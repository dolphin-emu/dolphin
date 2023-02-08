#ifndef VERSION_COMPARISON_FUNCTIONS
#define VERSION_COMPARISON_FUNCTIONS
#include <string>

namespace Lua
{
int compareFirstVersionToSecondVersion(std::string firstVersion, std::string secondVersion);
bool isFirstVersionGreaterThanSecondVersion(const std::string& firstVersion, const std::string& secondVersion);
bool isFirstVersionGreaterThanOrEqualToSecondVersion(const std::string& firstVersion, const std::string& secondVersion);
}  // namespace Lua
#endif
