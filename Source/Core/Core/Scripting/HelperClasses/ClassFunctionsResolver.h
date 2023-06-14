#pragma once
#include <string>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting
{
ClassMetadata GetClassMetadataForModule(const std::string& module_name, const std::string& version_number);
FunctionMetadata GetFunctionMetadataForModuleFunctionAndVersion(const std::string& module_name,
                                                                const std::string& function_name,
                                                                const std::string& version_number);

void Send_ClassMetadataForVersion_To_DLL_Buffer_impl(void*, const char*, const char*);  // Takes a ScriptContext* as 1st param, an input module name as 2nd param, andversion number as 3rd param. Calls the dll-specific DLLClassMetadataCopyHook function contained in the ScriptContext*, passing in the resulting opaque ClassMetadata* as a handle (allows for its fields to be copied to the DLL-side).
void Send_FunctionMetadataForVersion_To_DLL_Buffer_impl(void*, const char*, const char*, const char*);  // Takes a ScriptContext* as 1st param, an input module name as 2nd param, version number as 3rd param, and function name as 4th param. Calls the  dll-specific DLLFunctionMetadataCopyHook function contained in the ScriptContext*, passing in the resulting opaque FunctionMetadata* as a handle(allows for its fields to be copied to the DLL-side).
}
