// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32

#include <optional>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <devpropdef.h>

namespace Common
{
// Obtains a device property and returns it as a wide string.
std::wstring GetDeviceProperty(const HANDLE& device_info, const PSP_DEVINFO_DATA device_data,
                               const DEVPROPKEY* requested_property);

std::optional<std::wstring> GetDevNodeStringProperty(DEVINST device,
                                                     const DEVPROPKEY* requested_property);

std::optional<std::wstring> GetDeviceInterfaceStringProperty(LPCWSTR iface,
                                                             const DEVPROPKEY* requested_property);

}  // namespace Common

#endif
