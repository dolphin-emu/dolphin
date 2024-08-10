// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace QtUtils
{
inline QString FromStdString(std::string_view s)
{
  return QString::fromUtf8(s.data(), s.size());
}
inline QString FromStdString(std::u8string_view s)
{
  return QString::fromUtf8(s.data(), s.size());
}
inline QString FromStdString(std::u16string_view s)
{
  return QString::fromUtf16(s.data(), s.size());
}
}  // namespace QtUtils
