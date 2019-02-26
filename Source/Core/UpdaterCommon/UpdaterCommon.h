// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

extern FILE* log_fp;

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
    Manifest::Hash hash;
  };
  std::vector<DownloadOp> to_download;

  struct UpdateOp
  {
    Manifest::Filename filename;
    std::optional<Manifest::Hash> old_hash;
    Manifest::Hash new_hash;
  };
  std::vector<UpdateOp> to_update;

  struct DeleteOp
  {
    Manifest::Filename filename;
    Manifest::Hash old_hash;
  };
  std::vector<DeleteOp> to_delete;

  void Log() const;
};

void FatalError(const std::string& message);
std::optional<Manifest> FetchAndParseManifest(const std::string& url);
TodoList ComputeActionsToDo(Manifest this_manifest, Manifest next_manifest);
std::optional<std::string> FindOrCreateTempDir(const std::string& base_path);
void CleanUpTempDir(const std::string& temp_dir, const TodoList& todo);
bool PerformUpdate(const TodoList& todo, const std::string& install_base_path,
                   const std::string& content_base_url, const std::string& temp_path);
void FlushLog();
