#pragma once

#include "Core/Scripting/CoreScriptInterface/InternalAPIs/FileUtility_APIs.h"

// This file contains the implementations for the APIs in FileUtility_APIs
namespace Scripting::FileUtilityAPIsImpl
{
extern FileUtility_APIs fileUtility_APIs;

void Init();

const char* GetUserDirectoryPath_impl();
const char* GetSystemDirectoryPath_impl();

}  // namespace Scripting::FileUtilityAPIsImpl
