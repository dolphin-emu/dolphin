// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

class QWidget;

namespace WiiUpdate
{
void PerformOnlineUpdate(const std::string& region, QWidget* parent = nullptr);
void PerformDiscUpdate(const std::string& file_path, QWidget* parent = nullptr);
}  // namespace WiiUpdate
