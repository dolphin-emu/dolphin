// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/PEVersion.h"

class VCRuntimeUpdater
{
public:
  VCRuntimeUpdater();
  static std::optional<PEVersion> GetInstalledVersion();
  bool InstalledVersionIsOutdated();
  bool DownloadAndRun(const std::optional<std::string>& url = {}, bool quiet = false);
  bool Run(bool quiet = false);

private:
  bool CacheNeedsSync(const std::string& url);
  bool CacheSync(const std::optional<std::string>& url = {});

  std::filesystem::path redist_path;
};
