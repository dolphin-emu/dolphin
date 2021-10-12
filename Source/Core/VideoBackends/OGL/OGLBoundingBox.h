// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace OGL
{
class BoundingBox
{
public:
  static void Init();
  static void Shutdown();

  static void Flush();
  static void Readback();

  static void Set(int index, int value);
  static int Get(int index);
};
};  // namespace OGL
