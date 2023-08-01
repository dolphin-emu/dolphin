#pragma once

#include <string>

// This file contains the implementations for the APIs in FileUtility_APIs
namespace Scripting
{

// These 2 functions are called once at startup to set the user and system paths variables to their
// actual values.
void SetUserPath(std::string new_user_path);
void SetSysPath(std::string new_sys_path);

const char* GetUserDirectoryPath_impl();
const char* GetSystemDirectoryPath_impl();

}  // namespace Scripting
