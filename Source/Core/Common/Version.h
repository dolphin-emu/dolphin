// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace Common
{
const std::string& GetScmDescStr();
const std::string& GetScmBranchStr();
const std::string& GetSemVerStr();
const std::string& GetScmRevStr();
const std::string& GetScmRevGitStr();
const std::string& GetUserAgentStr();
const std::string& GetScmDistributorStr();
const std::string& GetScmUpdateTrackStr();
const std::string& GetNetplayDolphinVer();
int GetScmCommitsAheadMaster();
}  // namespace Common
