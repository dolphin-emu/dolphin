// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SWBoundingBox.h"

#include <algorithm>
#include <array>

#include "Common/CommonTypes.h"

namespace BBoxManager
{
namespace
{
// Current bounding box coordinates.
std::array<u16, 4> s_coordinates{};
}  // Anonymous namespace

u16 GetCoordinate(Coordinate coordinate)
{
  return s_coordinates[static_cast<u32>(coordinate)];
}

void SetCoordinate(Coordinate coordinate, const u16 value)
{
  s_coordinates[static_cast<u32>(coordinate)] = value;
}

void Update(const u16 left, const u16 right, const u16 top, const u16 bottom)
{
  const u16 new_left = std::min(left, GetCoordinate(Coordinate::Left));
  const u16 new_right = std::max(right, GetCoordinate(Coordinate::Right));
  const u16 new_top = std::min(top, GetCoordinate(Coordinate::Top));
  const u16 new_bottom = std::max(bottom, GetCoordinate(Coordinate::Bottom));

  SetCoordinate(Coordinate::Left, new_left);
  SetCoordinate(Coordinate::Right, new_right);
  SetCoordinate(Coordinate::Top, new_top);
  SetCoordinate(Coordinate::Bottom, new_bottom);
}

}  // namespace BBoxManager

namespace SW
{
std::vector<BBoxType> SWBoundingBox::Read(const u32 index, const u32 length)
{
  std::vector<BBoxType> values(length);

  for (u32 i = 0; i < length; i++)
  {
    values[i] = GetCoordinate(static_cast<BBoxManager::Coordinate>(index + i));
  }

  return values;
}

void SWBoundingBox::Write(const u32 index, const std::span<const BBoxType> values)
{
  for (size_t i = 0; i < values.size(); i++)
  {
    SetCoordinate(static_cast<BBoxManager::Coordinate>(index + i), values[i]);
  }
}

}  // namespace SW
