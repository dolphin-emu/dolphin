#pragma once

#include <Windows.h>
#include <string>

#include "Common/CommonTypes.h"

namespace WindowsRegistry
{
template <typename T>
bool ReadValue(T* value, const std::string& subkey, const std::string& name);
template bool ReadValue(u32* value, const std::string& subkey, const std::string& name);
template bool ReadValue(u64* value, const std::string& subkey, const std::string& name);
template <>
bool ReadValue(std::string* value, const std::string& subkey, const std::string& name);

OSVERSIONINFOW GetOSVersion();
};  // namespace WindowsRegistry
