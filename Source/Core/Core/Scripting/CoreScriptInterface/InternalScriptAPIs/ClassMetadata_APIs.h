#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// ClassMetadata_APIs contains the APIs which are used to get the fields stored in an opaque
// ClassMetadata* handle. The void* in each function below is the opaque ClassMetadata* handle.

// WARNING: These APIs should only be invoked on the DLL-side inside of the DLL's implementation of
// the DLL_Defined_ScriptContext_APIs.DLLClassMetadataCopyHook function
typedef struct ClassMetadata_APIs
{
  // WARNING: This function returns an exact copy of the name field of the ClassMetadata*, so it's
  // lifespan will end when the ClassMetadata* that the function was called with goes out of scope
  // on the Dolphin side.

  // Takes an opaque handle for a ClassMetadata* as input, and returns the const char* representing
  // the class name.
  const char* (*GetClassNameForClassMetadata)(void*);

  // Takes an opaque handle for a ClassMetadata* as input, and returns the number of
  // FunctionMetadataDefinitions contained in the class.
  unsigned long long (*GetNumberOfFunctions)(void*);

  // WARNING: This function returns an exact copy of the FunctionMetadata* stored in the
  // ClassMetadata*, so it's lifespan will end when the ClassMetadata* that the function was called
  // with goes out of scope on the Dolphin side.

  // Takes as input an opaque handle to a ClassMetadata* as its 1st param, and an index into the
  // array of FunctionMetadata*'s as its 2nd param (this array is 0-indexed, like regular C arrays).
  // Returns an opaque handle to the FunctionMetadata* stored at the specified position in the array
  // of FunctionMetadata objects contained in the ClassMetadata*.
  void* (*GetFunctionMetadataAtPosition)(void*, unsigned int);

} ClassMetadata_APIs;

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
