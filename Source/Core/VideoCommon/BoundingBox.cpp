// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/BoundingBox.h"

#include <algorithm>
#include <array>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/PixelShaderManager.h"

namespace BoundingBox
{
namespace
{
// Whether or not bounding box is enabled.
bool s_is_active = false;

// Current bounding box coordinates.
std::array<u16, 4> s_coordinates{
    0x80,
    0xA0,
    0x80,
    0xA0,
};
}  // Anonymous namespace

void Enable()
{
  s_is_active = true;
  PixelShaderManager::SetBoundingBoxActive(s_is_active);
}

void Disable()
{
  s_is_active = false;
  PixelShaderManager::SetBoundingBoxActive(s_is_active);
}

bool IsEnabled()
{
  return s_is_active;
}

u16 GetCoordinate(Coordinate coordinate)
{
  return s_coordinates[static_cast<u32>(coordinate)];
}

void SetCoordinate(Coordinate coordinate, u16 value)
{
  s_coordinates[static_cast<u32>(coordinate)] = value;
}

void Update(u16 left, u16 right, u16 top, u16 bottom)
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

void DoState(PointerWrap& p)
{
  p.Do(s_is_active);
  p.DoArray(s_coordinates);
}

}  // namespace BoundingBox
