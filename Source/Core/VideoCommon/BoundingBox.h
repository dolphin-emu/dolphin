// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

// Bounding Box manager
namespace BoundingBox
{
// Indicates a coordinate of the bounding box.
enum class Coordinate
{
  Left,    // The X coordinate of the left side of the bounding box.
  Right,   // The X coordinate of the right side of the bounding box.
  Top,     // The Y coordinate of the top of the bounding box.
  Bottom,  // The Y coordinate of the bottom of the bounding box.
};

// Enables bounding box.
void Enable();

// Disables bounding box.
void Disable();

// Determines if bounding box is enabled.
bool IsEnabled();

// Gets a particular coordinate for the bounding box.
u16 GetCoordinate(Coordinate coordinate);

// Sets a particular coordinate for the bounding box.
void SetCoordinate(Coordinate coordinate, u16 value);

// Updates all bounding box coordinates.
void Update(u16 left, u16 right, u16 top, u16 bottom);

// Save state
void DoState(PointerWrap& p);
}  // namespace BoundingBox
