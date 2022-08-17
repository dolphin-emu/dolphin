// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

struct Options
{
  std::string this_manifest_url;
  std::string next_manifest_url;
  std::string content_store_url;
  std::string install_base_path;
  std::optional<std::string> binary_to_restart;
  std::optional<u32> parent_pid;
  std::optional<std::string> log_file;
};

bool RunUpdater(const Options& options);
std::optional<Options> ParseCommandLine(const std::vector<std::string>& args);
