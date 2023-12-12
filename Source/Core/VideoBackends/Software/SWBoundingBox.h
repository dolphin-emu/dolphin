// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/BoundingBox.h"

namespace BBoxManager
{
// Indicates a coordinate of the bounding box.
enum class Coordinate
{
  Left,    // The X coordinate of the left side of the bounding box.
  Right,   // The X coordinate of the right side of the bounding box.
  Top,     // The Y coordinate of the top of the bounding box.
  Bottom,  // The Y coordinate of the bottom of the bounding box.
};

// Gets a particular coordinate for the bounding box.
u16 GetCoordinate(Coordinate coordinate);

// Sets a particular coordinate for the bounding box.
void SetCoordinate(Coordinate coordinate, u16 value);

// Updates all bounding box coordinates.
void Update(u16 left, u16 right, u16 top, u16 bottom);
}  // namespace BBoxManager

namespace SW
{
class SWBoundingBox final : public BoundingBox
{
public:
  bool Initialize() override { return true; }

protected:
  std::vector<BBoxType> Read(u32 index, u32 length) override;
  void Write(u32 index, std::span<const BBoxType> values) override;
};

}  // namespace SW
