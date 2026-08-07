#pragma once

#include "Common/CommonTypes.h"

#include <windows.h>

#include <string>

namespace WindowsRegistry
{
template <typename T>
bool ReadValue(T* value, const std::string& subkey, const std::string& name);
template <>
bool ReadValue(std::string* value, const std::string& subkey, const std::string& name);

OSVERSIONINFOW GetOSVersion();
}  // namespace WindowsRegistry
