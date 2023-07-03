#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
namespace Scripting
{

FunctionMetadata* castToFunctionMetadataPtr(void* input)
{
  return reinterpret_cast<FunctionMetadata*>(input);
}

const char* GetFunctionName_FunctionMetadta_impl(void* function_metadata)
{
  return castToFunctionMetadataPtr(function_metadata)->function_name.c_str();
}

const char* GetFunctionVersion_FunctionMetadata_impl(void* function_metadata)
{
  return castToFunctionMetadataPtr(function_metadata)->function_version.c_str();
}

const char* GetExampleFunctionCall_FunctionMetadata_impl(void* function_metadata)
{
  return castToFunctionMetadataPtr(function_metadata)->example_function_call.c_str();
}

// This function actually takes a ScriptContext* and a std::vector<ArgHolder*>* as its 2 inputs,
//  and returns an ArgHolder*.
// However, it's safe to cast this function to a different function type, as long as we cast it back
// to its original type before calling it. As such, we return this casted function pointer here, and
// the user calls the RunFunctionMain function with this casted function pointer as an argument.
// RunFunctionMain then casts the function pointer back to its original type before calling it.

// This is why the user can't directly invoke the function pointer returned by this function!!!
FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API
GetFunctionPointer_FunctionMetadata_impl(void* function_metadata)
{
  return reinterpret_cast<FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API>(
      castToFunctionMetadataPtr(function_metadata)->function_pointer);
}

int GetReturnType_FunctionMetadata_impl(void* function_metadata)
{
  return (int)castToFunctionMetadataPtr(function_metadata)->return_type;
}

unsigned long long GetNumberOfArguments_FunctionMetadata_impl(void* function_metadata)
{
  return castToFunctionMetadataPtr(function_metadata)->arguments_list.size();
}

int GetArgTypeEnumAtIndexInArguments_FunctionMetadata_impl(void* function_metadata,
                                                           unsigned int index)
{
  return castToFunctionMetadataPtr(function_metadata)->arguments_list.at(index);
}

typedef ArgHolder* (*REAL_FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA)(ScriptContext*,
                                                                       std::vector<ArgHolder*>*);
//  Actually calls the Script API function, and returns the result as an ArgHolder*
void* RunFunctionMain_impl(FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA_API function_handle,
                           void* script_context, void* ptr_to_vector_of_arg_holder_ptrs)
{
  return reinterpret_cast<void*>(
      reinterpret_cast<REAL_FUNCTION_POINTER_TYPE_FOR_FUNCTION_METADATA>(function_handle)(
          reinterpret_cast<ScriptContext*>(script_context),
          reinterpret_cast<std::vector<ArgHolder*>*>(ptr_to_vector_of_arg_holder_ptrs)));
}
}  // namespace Scripting
