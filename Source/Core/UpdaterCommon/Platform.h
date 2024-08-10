// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "UpdaterCommon/UpdaterCommon.h"

namespace Platform
{
bool VersionCheck(const std::vector<TodoList::UpdateOp>& to_update,
                  const std::string& install_base_path, const std::string& temp_dir);
}  // namespace Platform
