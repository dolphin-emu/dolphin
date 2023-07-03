#ifndef CLASS_FUNCTIONS_RESOLVER_APIS
#define CLASS_FUNCTIONS_RESOLVER_APIS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ClassFunctionsResolver_APIs
{
  // This function is called by the DLL. Takes a ScriptContext* as 1st param, an input module name
  // as 2nd param, and version number as 3rd param. Gets the ClassMetadata* for the specified
  // module+version, and then calls the DLL-specific DLLClassMetadataCopyHook function contained in
  // the ScriptContext*, passing in the resulting opaque ClassMetadata* as a handle (allows for its
  // fields to be copied to the DLL-side).
  void (*Send_ClassMetadataForVersion_To_DLL_Buffer)(void*, const char*, const char*);

  // This function is called by the DLL. Takes a ScriptContext* as 1st param, an input module name
  // as 2nd param, version number as 3rd param, and function name as 4th param. Gets the
  // FunctionMetadata* for the specified module+version+function name, and then calls the
  // DLL-specific DLLFunctionMetadataCopyHook function contained in the ScriptContext*, passing in
  // the resulting opaque FunctionMetadata* as a handle (allows for its fields to be copied to the
  // DLL-side).
  void (*Send_FunctionMetadataForVersion_To_DLL_Buffer)(void*, const char*, const char*,
                                                        const char*);

} ClassFunctionsResolver_APIs;

#ifdef __cplusplus
}
#endif

#endif
