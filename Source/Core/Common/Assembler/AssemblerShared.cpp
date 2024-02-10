// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assembler/AssemblerShared.h"

#include <fmt/format.h>

namespace Common::GekkoAssembler
{
std::string AssemblerError::FormatError() const
{
  const char* space_char = col == 0 ? "" : " ";

  std::string_view line_str = error_line;
  if (line_str.back() == '\n')
  {
    line_str = line_str.substr(0, line_str.length() - 1);
  }

  return fmt::format("Error on line {0} col {1}:\n"
                     "  {2}\n"
                     "  {3:{4}}{5:^^{6}}\n"
                     "{7}",
                     line + 1, col + 1, line_str, space_char, col, '^', len, message);
}
}  // namespace Common::GekkoAssembler
