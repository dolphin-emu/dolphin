#pragma once

#include <string>
#include "Core/Scripting/CoreScriptInterface/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"

#include "Core/Scripting/HelperClasses/ScriptContext.h"

// This file contains the implementation for the FunctionMetadata class, and the implementations for
// the APIs in FunctionMetadata_APIs
namespace Scripting
{
class FunctionMetadata
{
public:
  FunctionMetadata() = default;

  FunctionMetadata(const char* new_func_name, const char* new_func_version,
                   const char* new_example_function_call,
                   ArgHolder* (*new_function_ptr)(ScriptContext*, std::vector<ArgHolder*>*),
                   ArgTypeEnum new_return_type, std::vector<ArgTypeEnum> new_arguments_list)
  {
    function_name = std::string(new_func_name);
    function_version = std::string(new_func_version);
    example_function_call = std::string(new_example_function_call);
    function_pointer = new_function_ptr;
    return_type = new_return_type;
    arguments_list = std::move(new_arguments_list);
  }

  std::string function_name{};
  std::string function_version{};
  std::string example_function_call{};
  ArgHolder* (*function_pointer)(ScriptContext*, std::vector<ArgHolder*>*){};
  ArgTypeEnum return_type{};
  std::vector<ArgTypeEnum> arguments_list{};
};

using FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API = void* (*)(void*, void*);
const char* GetFunctionName_impl(void*);
const char* GetFunctionVersion_impl(void*);
const char* GetExampleFunctionCall_impl(void*);
int GetReturnType_impl(void*);
unsigned long long GetNumberOfArguments_impl(void*);
int GetTypeOfArgumentAtIndex_impl(void*, unsigned int);
FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API GetFunctionPointer_impl(void*);

// Where all API function calls occur from
void* RunFunction_impl(FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API, void*, void*);

}  // namespace Scripting
