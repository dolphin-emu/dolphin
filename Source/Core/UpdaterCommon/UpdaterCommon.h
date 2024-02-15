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

struct Manifest
{
  using Filename = std::string;
  using Hash = std::array<u8, 16>;
  std::map<Filename, Hash> entries;
};

// Represent the operations to be performed by the updater.
struct TodoList
{
  struct DownloadOp
  {
    Manifest::Filename filename;
    Manifest::Hash hash{};
  };
  std::vector<DownloadOp> to_download;

  struct UpdateOp
  {
    Manifest::Filename filename;
    std::optional<Manifest::Hash> old_hash;
    Manifest::Hash new_hash{};
  };
  std::vector<UpdateOp> to_update;

  struct DeleteOp
  {
    Manifest::Filename filename;
    Manifest::Hash old_hash{};
  };
  std::vector<DeleteOp> to_delete;

  void Log() const;
};

void LogToFile(const char* fmt, ...);
std::string HexEncode(const u8* buffer, size_t size);
Manifest::Hash ComputeHash(const std::string& contents);
bool RunUpdater(std::vector<std::string> args);
