// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace OGL
{
class BoundingBox
{
public:
  static void Init();
  static void Shutdown();

  static void Set(int index, int value);
  static int Get(int index);
};
};  // namespace OGL
