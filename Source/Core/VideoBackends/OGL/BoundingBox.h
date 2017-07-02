// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace OGL
{
class BoundingBox
{
public:
  static void Init(int target_width, int target_height);
  static void Shutdown();

  static void SetTargetSizeChanged(int target_width, int target_height);

  // When SSBO isn't available, the bounding box is calculated directly from the
  // stencil buffer.
  static bool NeedsStencilBuffer();
  // When the stencil buffer is changed, this function needs to be called to
  // invalidate the cached bounding box data.
  static void StencilWasUpdated();

  static void Set(int index, int value);
  static int Get(int index);
};
};
