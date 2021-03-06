// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

namespace Config
{
// Layers in ascending order of priority.
enum class LayerType
{
  Base,
  CommandLine,
  GlobalGame,
  LocalGame,
  Movie,
  Netplay,
  CurrentRun,
  Meta,
};

enum class System
{
  Main,
  SYSCONF,
  GCPad,
  WiiPad,
  GCKeyboard,
  GFX,
  Logger,
  Debugger,
  DualShockUDPClient,
  FreeLook,
};

constexpr std::array<LayerType, 7> SEARCH_ORDER{{
    LayerType::CurrentRun,
    LayerType::Netplay,
    LayerType::Movie,
    LayerType::LocalGame,
    LayerType::GlobalGame,
    LayerType::CommandLine,
    LayerType::Base,
}};
}  // namespace Config
