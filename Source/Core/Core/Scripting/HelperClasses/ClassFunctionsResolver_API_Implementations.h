#pragma once

#include <string>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

// This file contains the implementations for the APIs in ClassFunctionsResolver_APIs
namespace Scripting
{

void Send_ClassMetadataForVersion_To_DLL_Buffer_impl(void*, const char*, const char*);
void Send_FunctionMetadataForVersion_To_DLL_Buffer_impl(void*, const char*, const char*,
                                                        const char*);

}  // namespace Scripting
