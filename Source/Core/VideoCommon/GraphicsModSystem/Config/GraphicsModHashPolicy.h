// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <type_traits>

#include "Common/CommonTypes.h"

namespace GraphicsModSystem::Config
{
constexpr u16 NoHashAttributes = 0;
enum class HashAttributes : u16
{
  Blending = 1u << 1,
  Projection = 1u << 2,
  VertexPosition = 1u << 3,
  VertexTexCoords = 1u << 4,
  VertexLayout = 1u << 5,
  Indices = 1u << 6
};

struct HashPolicy
{
  HashAttributes attributes;
  bool first_texture_only;
  u64 version;
};

HashAttributes operator|(HashAttributes lhs, HashAttributes rhs);
HashAttributes operator&(HashAttributes lhs, HashAttributes rhs);

HashPolicy GetDefaultHashPolicy();
HashAttributes HashAttributesFromString(const std::string& str);
std::string HashAttributesToString(HashAttributes attributes);

}  // namespace GraphicsModSystem::Config
