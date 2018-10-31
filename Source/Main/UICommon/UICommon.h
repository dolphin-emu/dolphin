// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace UICommon
{
void Init();
void Shutdown();

#if defined(HAVE_XRANDR) && HAVE_XRANDR
void EnableScreenSaver(unsigned long win, bool enable);
#else
void EnableScreenSaver(bool enable);
#endif

// Calls std::locale::global, selecting a fallback locale if the
// requested locale isn't available
void SetLocale(std::string locale_name);

void CreateDirectories();
void SetUserDirectory(const std::string& custom_path);

bool TriggerSTMPowerEvent();

void SaveWiimoteSources();

// Return a pretty file size string from byte count.
// e.g. 1134278 -> "1.08 MiB"
std::string FormatSize(u64 bytes);
}  // namespace UICommon
