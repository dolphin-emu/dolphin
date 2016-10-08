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
  static void StoreFrameData(const u8* data, int width, int height, int stride);

public:
  static bool Start(int w, int h);
  static void AddFrame(const u8* data, int width, int height, int stride);
  static void Stop();
  static void DoState();
};
