#ifndef FUNCTION_METADATA_APIS
#define FUNCTION_METADATA_APIS

#ifdef __cplusplus
extern "C" {
#endif

// WARNING: These APIs should only be invoked on the DLL-side inside of the function that implements
// the DLL_Defined_ScriptContext_APIs.DLLClassMetadataCopyHook function OR the
// DLL_Defined_ScriptContext_APIs.DLLFunctionMetadataCopyHook function.

typedef struct FunctionMetadata_APIs
{
  typedef void* (*opaque_function_type)(void*, void*);

  // WARNING: All of these functions that return const char* have a return result that is only valid
  // until the FunctionMetadata* passed in as input goes out of scope.
  // --------------------------------------------------------------------------------------------
  // Takes an opaque handle for a FunctionMetadata* as input, and returns the function's name.
  const char* (*GetFunctionName)(void*);

  // Takes an opaque handle for a FunctionMetadata* as input, and returns the version that the
  // FunctionMetadata* was first defined in.
  const char* (*GetFunctionVersion)(void*);

  // Takes an opaque handle for a FunctionMetadata* as input, and returns an example function call
  // for the FunctionMetadata.
  const char* (*GetExampleFunctionCall)(void*);

  // Takes an opaque handle for a FunctionMetadata* as input, and returns an int which
  // represents the return type of the function (the int is of type ScriptingEnums::ArgTypeEnum).
  int (*GetReturnType)(void*);

  // Takes an opaque handle for a FunctionMetadata* as input, and returns the total
  // number of arguments that the function takes.
  unsigned long long (*GetNumberOfArguments)(void*);

  // Takes an opaque handle to a FunctionMetadata* as its 1st input, and an index into the array of
  // argument types for the function as it's 2nd argument (is a 0-indexed array). This function then
  // returns an int which represents the type of the argument at the specified position for the
  // function (the int is of type ScriptingEnums::ArgTypeEnum).
  int (*GetTypeOfArgumentAtIndex)(void*, unsigned int);

  // DOUBLE WARNING: DO NOT call the return result of this function directly. Instead, pass it in as
  // an argument to the RunFunction function, which will handle casting+calling the function.
  // --------------------------------------------------------------------------------------------
  // Takes as input an opaque handle to a FunctionMetadata*, and returns the actual
  // pointer to the location in code that the function is stored at.
  opaque_function_type (*GetFunctionPointer)(void*);

  // WARNING: The ArgHolder* delete function must be called on the return result of this function.
  // --------------------------------------------------------------------------------------------
  // Takes as input a function pointer as its 1st param (the return result of calling
  // GetFunctionPointer), a ScriptContext* as its 2nd param, and an opaque handle to a
  // std::vector<ArgHolder*>* as its 3rd argument. This function calls the function passed into it
  // with the specified arguments, and returns the resulting ArgHolder* returned by the function
  // call as a void*.
  void* (*RunFunction)(opaque_function_type, void*, void*);

} FunctionMetadata_APIs;

#ifdef __cplusplus
}
#endif

#endif
