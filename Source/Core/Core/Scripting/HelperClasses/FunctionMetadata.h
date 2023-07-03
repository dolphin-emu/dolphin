#pragma once
#include <string>
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"

#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting
{
class FunctionMetadata
{
public:
  FunctionMetadata()
  {
    this->function_name = "";
    this->function_version = "";
    this->example_function_call = "";
    function_pointer = nullptr;
    return_type = (ScriptingEnums::ArgTypeEnum)0;
    arguments_list = std::vector<ScriptingEnums::ArgTypeEnum>();
  }

  FunctionMetadata(const char* new_func_name, const char* new_func_version,
                   const char* new_example_function_call,
                   ArgHolder* (*new_function_ptr)(ScriptContext*, std::vector<ArgHolder*>*),
                   ScriptingEnums::ArgTypeEnum new_return_type,
                   std::vector<ScriptingEnums::ArgTypeEnum> new_arguments_list)
  {
    this->function_name = std::string(new_func_name);
    this->function_version = std::string(new_func_version);
    this->example_function_call = std::string(new_example_function_call);
    this->function_pointer = new_function_ptr;
    this->return_type = new_return_type;
    this->arguments_list = new_arguments_list;
  }

  std::string function_name;
  std::string function_version;
  std::string example_function_call;
  ArgHolder* (*function_pointer)(ScriptContext*, std::vector<ArgHolder*>*);
  ScriptingEnums::ArgTypeEnum return_type;
  std::vector<ScriptingEnums::ArgTypeEnum> arguments_list;
};

typedef void* (*FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API)(void*, void*);

const char* GetFunctionName_FunctionMetadta_impl(void*);
const char* GetFunctionVersion_FunctionMetadata_impl(void*);
const char* GetExampleFunctionCall_FunctionMetadata_impl(void*);

FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API GetFunctionPointer_FunctionMetadata_impl(void*);
int GetReturnType_FunctionMetadata_impl(void*);
unsigned long long GetNumberOfArguments_FunctionMetadata_impl(void*);
int GetArgTypeEnumAtIndexInArguments_FunctionMetadata_impl(void*, unsigned int);

// Where all API function calls occur from
void* RunFunctionMain_impl(FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API, void*, void*);

}  // namespace Scripting
