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

const char* GetClassName_ClassMetadata_impl(void*);
unsigned long long GetNumberOfFunctions_ClassMetadata_impl(void*);
void* GetFunctionMetadataAtPosition_ClassMetadata_impl(void*, unsigned int);
}  // namespace Scripting
