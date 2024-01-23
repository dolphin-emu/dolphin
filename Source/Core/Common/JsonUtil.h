// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include <picojson.h>

// Ideally this would use a concept like, 'template <std::ranges::range Range>' to constrain it,
// but unfortunately we'd need to require clang 15 for that, since the ranges library isn't
// fully implemented until then, but this should suffice.

template <typename Range>
picojson::array ToJsonArray(const Range& data)
{
  picojson::array result;
  result.reserve(std::size(data));

  for (const auto& value : data)
  {
    result.emplace_back(static_cast<double>(value));
  }

  return result;
}
