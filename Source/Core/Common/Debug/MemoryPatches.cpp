// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Debug/MemoryPatches.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace Common::Debug
{
MemoryPatch::MemoryPatch(u32 address_, std::vector<u8> value_)
    : address(address_), value(std::move(value_))
{
}

MemoryPatch::MemoryPatch(u32 address_, u32 value_)
    : MemoryPatch(address_, {static_cast<u8>(value_ >> 24), static_cast<u8>(value_ >> 16),
                             static_cast<u8>(value_ >> 8), static_cast<u8>(value_)})
{
}

MemoryPatches::MemoryPatches() = default;
MemoryPatches::~MemoryPatches() = default;

void MemoryPatches::SetPatch(const Core::CPUThreadGuard& guard, u32 address, u32 value)
{
  const std::size_t index = m_patches.size();
  m_patches.emplace_back(address, value);
  Patch(guard, index);
}

void MemoryPatches::SetPatch(const Core::CPUThreadGuard& guard, u32 address, std::vector<u8> value)
{
  UnsetPatch(guard, address);
  const std::size_t index = m_patches.size();
  m_patches.emplace_back(address, std::move(value));
  Patch(guard, index);
}

void MemoryPatches::SetFramePatch(const Core::CPUThreadGuard& guard, u32 address, u32 value)
{
  const std::size_t index = m_patches.size();
  m_patches.emplace_back(address, value);
  m_patches.back().type = MemoryPatch::ApplyType::EachFrame;
  Patch(guard, index);
}

void MemoryPatches::SetFramePatch(const Core::CPUThreadGuard& guard, u32 address,
                                  std::vector<u8> value)
{
  UnsetPatch(guard, address);
  const std::size_t index = m_patches.size();
  m_patches.emplace_back(address, std::move(value));
  m_patches.back().type = MemoryPatch::ApplyType::EachFrame;
  Patch(guard, index);
}

const std::vector<MemoryPatch>& MemoryPatches::GetPatches() const
{
  return m_patches;
}

void MemoryPatches::UnsetPatch(const Core::CPUThreadGuard& guard, u32 address)
{
  const auto it = std::find_if(m_patches.begin(), m_patches.end(),
                               [address](const auto& patch) { return patch.address == address; });

  if (it == m_patches.end())
    return;

  const std::size_t index = std::distance(m_patches.begin(), it);
  RemovePatch(guard, index);
}

void MemoryPatches::EnablePatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  if (m_patches[index].is_enabled == MemoryPatch::State::Enabled)
    return;
  m_patches[index].is_enabled = MemoryPatch::State::Enabled;
  Patch(guard, index);
}

void MemoryPatches::DisablePatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  if (m_patches[index].is_enabled == MemoryPatch::State::Disabled)
    return;
  m_patches[index].is_enabled = MemoryPatch::State::Disabled;
  Patch(guard, index);
}

bool MemoryPatches::HasEnabledPatch(u32 address) const
{
  return std::any_of(m_patches.begin(), m_patches.end(), [address](const MemoryPatch& patch) {
    return patch.address == address && patch.is_enabled == MemoryPatch::State::Enabled;
  });
}

void MemoryPatches::RemovePatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  DisablePatch(guard, index);
  UnPatch(index);
  m_patches.erase(m_patches.begin() + index);
}

void MemoryPatches::ClearPatches(const Core::CPUThreadGuard& guard)
{
  const std::size_t size = m_patches.size();
  for (std::size_t index = 0; index < size; ++index)
  {
    DisablePatch(guard, index);
    UnPatch(index);
  }
  m_patches.clear();
}
}  // namespace Common::Debug
