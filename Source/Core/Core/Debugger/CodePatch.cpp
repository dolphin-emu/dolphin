// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Debugger/CodePatch.h"

#include <utility>

#include "Core/PowerPC/PowerPC.h"

CodePatchBase::~CodePatchBase() = default;

CodePatch::CodePatch(u32 address, const std::vector<u8>& patch) : m_address(address), m_patch(patch)
{
}

CodePatch::CodePatch(u32 address, std::vector<u8>&& patch)
    : m_address(address), m_patch(std::move(patch))
{
}

CodePatch::~CodePatch() = default;

u32 CodePatch::GetAddress() const
{
  return m_address;
}

std::size_t CodePatch::GetSize() const
{
  return m_patch.size();
}

bool CodePatch::Apply()
{
  if (GetState() != State::Source)
    return false;

  std::size_t size = GetSize();
  for (std::size_t index = 0; index < size; ++index)
  {
    m_source.push_back(PowerPC::HostRead_U8(m_address + static_cast<u32>(index)));
    PowerPC::HostWrite_U8(m_patch[index], m_address + static_cast<u32>(index));
  }

  return true;
}

bool CodePatch::Remove()
{
  if (GetState() != State::Patch)
    return false;

  std::size_t size = GetSize();
  for (std::size_t index = 0; index < size; ++index)
    PowerPC::HostWrite_U8(m_source[index], m_address + static_cast<u32>(index));
  m_source.clear();

  return true;
}

CodePatch::State CodePatch::GetState() const
{
  if (m_source.empty())
    return State::Source;

  std::size_t size = GetSize();
  for (std::size_t index = 0; index < size; ++index)
  {
    if (PowerPC::HostRead_U8(m_address + static_cast<u32>(index)) != m_patch[index])
      return State::Invalid;
  }

  return State::Patch;
}
