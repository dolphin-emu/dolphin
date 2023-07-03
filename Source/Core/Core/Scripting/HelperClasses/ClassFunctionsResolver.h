#pragma once
#include <string>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting
{
ClassMetadata GetClassMetadataForModule(const std::string& module_name,
                                        const std::string& version_number);
FunctionMetadata GetFunctionMetadataForModuleFunctionAndVersion(const std::string& module_name,
                                                                const std::string& function_name,
                                                                const std::string& version_number);

void Send_ClassMetadataForVersion_To_DLL_Buffer_impl(void*, const char*, const char*);
void Send_FunctionMetadataForVersion_To_DLL_Buffer_impl(void*, const char*, const char*,
                                                        const char*);
}  // namespace Scripting
