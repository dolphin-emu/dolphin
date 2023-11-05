#pragma once

#include <string>

namespace Scripting
{

bool IsFirstVersionGreaterThanSecondVersion(const std::string& first_version,
                                            const std::string& second_version);
bool IsFirstVersionGreaterThanOrEqualToSecondVersion(const std::string& first_version,
                                                     const std::string& second_version);

}  // namespace Scripting
