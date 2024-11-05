// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <type_traits>

#include <picojson.h>

#include "Common/Matrix.h"

// Ideally this would use a concept like, 'template <std::ranges::range Range>' to constrain it,
// but unfortunately we'd need to require clang 15 for that, since the ranges library isn't
// fully implemented until then, but this should suffice.

template <typename Range>
picojson::array ToJsonArray(const Range& data)
{
  using RangeUnderlyingType = typename Range::value_type;
  picojson::array result;
  result.reserve(std::size(data));

  for (const auto& value : data)
  {
    if constexpr (std::is_integral_v<RangeUnderlyingType> ||
                  std::is_floating_point_v<RangeUnderlyingType>)
    {
      result.emplace_back(static_cast<double>(value));
    }
    else
    {
      result.emplace_back(value);
    }
  }

  return result;
}

template <typename Type>
std::optional<Type> ReadNumericFromJson(const picojson::object& obj, const std::string& key)
{
  const auto it = obj.find(key);
  if (it == obj.end())
    return std::nullopt;
  if (!it->second.is<double>())
    return std::nullopt;
  return static_cast<Type>(it->second.get<double>());
}

std::optional<std::string> ReadStringFromJson(const picojson::object& obj, const std::string& key);

std::optional<bool> ReadBoolFromJson(const picojson::object& obj, const std::string& key);

picojson::object ToJsonObject(const Common::Vec3& vec);
void FromJson(const picojson::object& obj, Common::Vec3& vec);

bool JsonToFile(const std::string& filename, const picojson::value& root, bool prettify = false);
bool JsonFromFile(const std::string& filename, picojson::value* root, std::string* error);
