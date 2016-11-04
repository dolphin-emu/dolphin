// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class AVIDump
{
private:
  static bool CreateFile();
  static void CloseFile();
  static void CheckResolution(int width, int height);

public:
  struct Frame
  {
    u64 ticks = 0;
    u32 ticks_per_second = 0;
    bool first_frame = false;
  };

  static bool Start(int w, int h);
  static void AddFrame(const u8* data, int width, int height, int stride, const Frame& state);
  static void Stop();
  static void DoState();
  static Frame FetchState(u64 ticks);
};
