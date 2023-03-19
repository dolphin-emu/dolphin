// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <sstream>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "UpdaterCommon/UpdaterCommon.h"

namespace Platform
{
bool VersionCheck(const std::vector<TodoList::UpdateOp>& to_update,
                  const std::string& install_base_path, const std::string& temp_dir);
}  // namespace Platform
