// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

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
  SYSCONF,
  GCPad,
  WiiPad,
  GCKeyboard,
  GFX,
  Logger,
  Debugger,
  UI,
};

constexpr std::array<LayerType, 7> SEARCH_ORDER{{
    // Skip the meta layer
    LayerType::CurrentRun, LayerType::CommandLine, LayerType::Movie, LayerType::Netplay,
    LayerType::LocalGame, LayerType::GlobalGame, LayerType::Base,
}};
}
