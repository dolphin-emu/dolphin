// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"

class CodePatchBase
{
public:
  enum class State
  {
    Source,
    Patch,
    Invalid
  };
  virtual ~CodePatchBase();
  virtual u32 GetAddress() const = 0;
  virtual std::size_t GetSize() const = 0;
  virtual bool Apply() = 0;
  virtual bool Remove() = 0;
  virtual State GetState() const = 0;
};

class CodePatch : public CodePatchBase
{
public:
  CodePatch(u32 address, const std::vector<u8>& patch);
  CodePatch(u32 address, std::vector<u8>&& patch);
  ~CodePatch();

  u32 GetAddress() const override;
  std::size_t GetSize() const override;
  bool Apply() override;
  bool Remove() override;
  State GetState() const override;

private:
  u32 m_address;
  std::vector<u8> m_patch;
  std::vector<u8> m_source;
};

using CodePatches = std::vector<CodePatch>;
