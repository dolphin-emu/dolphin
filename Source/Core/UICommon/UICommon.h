// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"

struct WindowSystemInfo;

namespace UICommon
{
void Init();
void Shutdown();

void InitControllers(const WindowSystemInfo& wsi);
void ShutdownControllers();

void InhibitScreenSaver(bool enable);

// Calls std::locale::global, selecting a fallback locale if the
// requested locale isn't available
void SetLocale(std::string locale_name);

void CreateDirectories();
void SetUserDirectory(std::string custom_path);

bool TriggerSTMPowerEvent();

// Return a pretty file size string from byte count.
// e.g. 1134278 -> "1.08 MiB"
std::string FormatSize(u64 bytes, int decimals = 2);
}  // namespace UICommon
