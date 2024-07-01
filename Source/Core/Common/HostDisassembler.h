// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>

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

  virtual void Disassemble(const u8* begin, const u8* end, std::ostream& stream,
                           std::size_t& instruction_count);
  void Disassemble(const u8* begin, const u8* end, std::ostream& stream);
  std::string Disassemble(const u8* begin, const u8* end);
};
