// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

class QWidget;

namespace WiiUpdate
{
void PerformOnlineUpdate(const std::string& region, QWidget* parent = nullptr);
void PerformDiscUpdate(const std::string& file_path, QWidget* parent = nullptr);
}  // namespace WiiUpdate
