// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <type_traits>

#include <nlohmann/json.hpp>

#include "Common/Matrix.h"

template <typename Type>
std::optional<Type> ReadNumericFromJson(const nlohmann::json& obj, const std::string_view key)
{
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_number())
    return std::nullopt;

  const double numeric = it->get<double>();

  if constexpr (std::is_same_v<Type, double>)
  {
    return numeric;
  }
  else
  {
    if (numeric < static_cast<double>(std::numeric_limits<Type>::lowest()) ||
        numeric > static_cast<double>(std::numeric_limits<Type>::max()))
    {
      return std::nullopt;
    }
    return static_cast<Type>(numeric);
  }
}

std::optional<std::string> ReadStringFromJson(const nlohmann::json& obj,
                                              const std::string_view key);

std::optional<bool> ReadBoolFromJson(const nlohmann::json& obj, const std::string_view key);

std::optional<nlohmann::json> ReadObjectFromJson(const nlohmann::json& obj,
                                                 const std::string_view key);

std::optional<nlohmann::json> ReadArrayFromJson(const nlohmann::json& obj,
                                                const std::string_view key);

nlohmann::json ToJsonObject(const Common::Vec3& vec);
void FromJson(const nlohmann::json& obj, Common::Vec3& vec);

bool JsonToFile(const std::string& filename, const nlohmann::json& root, bool prettify = false);
bool JsonFromFile(const std::string& filename, nlohmann::json* root, std::string* error);
