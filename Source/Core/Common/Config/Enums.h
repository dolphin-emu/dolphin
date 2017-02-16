// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Config
{
enum class LayerType
{
  Base,
  GlobalGame,
  LocalGame,
  Movie,
  Netplay,
  CommandLine,
  CurrentRun,
  Meta,
};

enum class System
{
  Main,
  GCPad,
  WiiPad,
  GCKeyboard,
  GFX,
  Logger,
  Debugger,
  UI,
};
}
