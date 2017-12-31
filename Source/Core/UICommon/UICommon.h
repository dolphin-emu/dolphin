// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

namespace UICommon
{
void Init();
void Shutdown();

#if defined(HAVE_XRANDR) && HAVE_XRANDR
void EnableScreenSaver(Display* display, Window win, bool enable);
#else
void EnableScreenSaver(bool enable);
#endif

void CreateDirectories();
void SetUserDirectory(const std::string& custom_path);

bool TriggerSTMPowerEvent();

void SaveWiimoteSources();

// Return a pretty file size string from byte count.
// e.g. 1134278 -> "1.08 MiB"
std::string FormatSize(u64 bytes);
}  // namespace UICommon
