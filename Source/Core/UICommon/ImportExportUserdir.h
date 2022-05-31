// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace Common
{
class Flag;
}

namespace UICommon
{
bool ExportUserDir(const std::string& archive_path, Common::Flag* cancel_requested_flag);
}  // namespace UICommon
