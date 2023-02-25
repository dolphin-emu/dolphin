#pragma once
#include <string>
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"

namespace Scripting
{
struct FunctionMetadata
{
  std::string function_name;
  std::string function_version;
  ArgHolder (*function_pointer)(std::vector<ArgHolder>&);
  ArgTypeEnum return_type;
  std::vector<ArgTypeEnum> arguments_list;
};
}  // namespace Scripting
