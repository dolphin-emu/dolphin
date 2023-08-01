#pragma once

#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

// This file contains the implementations for the APIs in ClassMetadata_APIs
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
