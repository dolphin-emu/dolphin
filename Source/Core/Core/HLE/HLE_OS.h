// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core
{
class CPUThreadGuard;
};

namespace HLE_OS
{
void HLE_GeneralDebugPrint(const Core::CPUThreadGuard& guard);
void HLE_GeneralDebugVPrint(const Core::CPUThreadGuard& guard);
void HLE_write_console(const Core::CPUThreadGuard& guard);
void HLE_OSPanic(const Core::CPUThreadGuard& guard);
void HLE_LogDPrint(const Core::CPUThreadGuard& guard);
void HLE_LogVDPrint(const Core::CPUThreadGuard& guard);
void HLE_LogFPrint(const Core::CPUThreadGuard& guard);
void HLE_LogVFPrint(const Core::CPUThreadGuard& guard);
}  // namespace HLE_OS
