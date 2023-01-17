// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common::Debug
{
struct MemoryPatch
{
  enum class State
  {
    Enabled,
    Disabled
  };

  enum class ApplyType
  {
    Once,
    EachFrame
  };

  MemoryPatch(u32 address_, std::vector<u8> value_);
  MemoryPatch(u32 address_, u32 value_);

  u32 address;
  std::vector<u8> value;
  State is_enabled = State::Enabled;
  ApplyType type = ApplyType::Once;
};

class MemoryPatches
{
public:
  MemoryPatches();
  virtual ~MemoryPatches();

  void SetPatch(u32 address, u32 value);
  void SetPatch(u32 address, std::vector<u8> value);
  void SetFramePatch(u32 address, u32 value);
  void SetFramePatch(u32 address, std::vector<u8> value);
  const std::vector<MemoryPatch>& GetPatches() const;
  void UnsetPatch(u32 address);
  void EnablePatch(std::size_t index);
  void DisablePatch(std::size_t index);
  bool HasEnabledPatch(u32 address) const;
  void RemovePatch(std::size_t index);
  void ClearPatches();
  virtual void ApplyExistingPatch(std::size_t index) = 0;

protected:
  virtual void Patch(std::size_t index) = 0;
  virtual void UnPatch(std::size_t index) = 0;

  std::vector<MemoryPatch> m_patches;
};
}  // namespace Common::Debug
