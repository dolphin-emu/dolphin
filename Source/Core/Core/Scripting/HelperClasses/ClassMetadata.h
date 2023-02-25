#pragma once
#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

namespace Scripting
{
struct ClassMetadata
{
  std::string class_name;
  std::vector<FunctionMetadata> functions_list;
};
}  // namespace Scripting
