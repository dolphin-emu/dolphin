// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <iosfwd>
#include <memory>

#include "Common/CommonTypes.h"

class HostDisassembler
{
public:
  enum class Platform
  {
    x86_64,
    aarch64,
  };

  virtual ~HostDisassembler() = default;

  static std::unique_ptr<HostDisassembler> Factory(Platform arch);

  virtual std::size_t Disassemble(const u8* begin, const u8* end, std::ostream& stream);
};
