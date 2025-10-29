// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace VideoCommon
{
class AssetListener
{
public:
  AssetListener() = default;
  virtual ~AssetListener() = default;

  AssetListener(const AssetListener&) = default;
  AssetListener(AssetListener&&) = default;
  AssetListener& operator=(const AssetListener&) = default;
  AssetListener& operator=(AssetListener&&) = default;

  virtual void NotifyAssetLoadSuccess() = 0;
  virtual void NotifyAssetLoadFailed() = 0;
  virtual void AssetUnloaded() = 0;
};
}  // namespace VideoCommon
