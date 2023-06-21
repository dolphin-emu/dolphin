#ifndef CLASS_METADATA_APIS
#define CLASS_METADATA_APIS

#ifdef __cplusplus
extern "C" {
#endif

// WARNING: These APIs should only be invoked on the DLL-side inside of the function that implements the
// DLL_Defined_ScriptContext_APIs.DLLClassMetadataCopyHook function

  typedef struct ClassMetadata_APIs
{
    // WARNING: This returns an exact copy of the name field of the ClassMetadata*, so it's lifespan
    // will end when the ClassMetadata* that the function was called with goes out of scope in the Dolphin side.
    const char* (*GetClassName)(void*);  // takes an opaque handle to a ClassMetadata* as input, and returns the const char* representing the class name.
    unsigned long long (*GetNumberOfFunctions)(void*);  // Takes an opaque handle to a ClassMetadata* as input, and returns the number of FunctionMetadataDefinitions contained in the class.

    //WARNING: This returns an exact copy of the FunctionMetadata* stored in the ClassMetadata*, so it's lifespan
    // will end when the ClassMetadata* that the function was called with goes out of scope in the Dolphin side.
    void* (*GetFunctionMetadataAtPosition)(void*, unsigned int);  // Takes as input an opaque handle to a ClassMetadata* as its 1st param, and an index into the array of FunctionMetadata functions as its 2nd param (this array starts at 0, like regular C arrays). Returns an opaque handle  to the FunctionMetadta* stored at the specified position in the array of FunctionMetadata objects contained in the ClassMetadata.
} ClassMetadata_APIs;

#ifdef __cplusplus
}
#endif

#endif
