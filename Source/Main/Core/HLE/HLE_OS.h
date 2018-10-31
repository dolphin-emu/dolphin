// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace HLE_OS
{
void HLE_GeneralDebugPrint();
void HLE_GeneralDebugVPrint();
void HLE_write_console();
void HLE_OSPanic();
void HLE_LogDPrint();
void HLE_LogVDPrint();
void HLE_LogFPrint();
void HLE_LogVFPrint();
}
