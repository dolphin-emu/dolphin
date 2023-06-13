#pragma once
#include <string>
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"

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
    return_type = (ArgTypeEnum)0;
    arguments_list = std::vector<ArgTypeEnum>();
  }

  FunctionMetadata(const char* new_func_name, const char* new_func_version,
                   const char* new_example_function_call,
                   ArgHolder* (*new_function_ptr)(ScriptContext*, std::vector<ArgHolder*>*),
                   ArgTypeEnum new_return_type, std::vector<ArgTypeEnum> new_arguments_list)
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
  ArgTypeEnum return_type;
  std::vector<ArgTypeEnum> arguments_list;
};

typedef void* (*FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API)(void*, void*);

// WARNING: The 1st pointer in each function's argument list is an opaque handle to a FunctionMetadata*.
// The function which return const char* only have their return value be valid for as long as the lifespan to
// the inputted FunctionMetadata* lasts on the Dolphin side.

// As such, the 6 functions below should only be invoked from the DLL inside its FunctionMetadata_HookCallback or its ClassMetadata_HookCallback
const char* GetFunctionName_FunctionMetadta_impl(void*); // Returns the name of the function for the FunctionMetadata*
const char* GetFunctionVersion_FunctionMetadata_impl(void*); // Returns the version of the function for the FunctionMetadata*
const char* GetExampleFunctionCall_FunctionMetadata_impl(void*); // Returns an example function call for the FunctionMetadata*

// WARNING: The return result of this function call should never be directly invoked by the DLL! Rather, it should pass the pointer returned by this function into the RunFunctionMain function, which will handle calling the function and returns its results
FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API GetFunctionPointer_FunctionMetadata_impl(void*); // Returns an opaque function pointer, which can be passed into the RunFunctionMain_impl function below.
int GetReturnType_FunctionMetadata_impl(void*); // Returns the return type of the FunctionMetadata*, which represents an ArgTypeEnum value.
unsigned int GetNumberOfArguments_FunctionMetadata_impl(void*); // Returns the number of arguments that the FunctionMetadata* takes as input.
int GetArgTypeEnumAtIndexInArguments_FunctionMetadata_impl(void*, unsigned int); // Returns the type of the argument in the list of arguments for the FunctionMetadata* at the specified index (starting with index 0). The return value  represents an ArgTypeEnum value.

void* RunFunctionMain_impl(FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API, void*, void*); // The function which is called in order to actually call the scripting function. Takes as its 1st param the pointer to the function (returned by calling GetFunctionPointer_FunctionMetadata), the ScriptContext* as its 2nd param, and the vector* of ArgHolder* as its 3rd param.
// The function returns the ArgHolder* which represents the result of the function call. Note that this ArgHolder allocates memory on the heap, so the ArgHolder destructor must be called on it.
// Also, after this function executes, it's safe to call the VectorOfArgHolders delete method on the inputted vector (the 3rd arg to the function above) (Don't call the ArgHolder_Destructor method on the individual components of the VectorOfArgHolder - let the VectorOfArgHolder destructor handle that!

}  // namespace Scripting
