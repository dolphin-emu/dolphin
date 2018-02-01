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
  VRGame,
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

constexpr std::array<LayerType, 8> SEARCH_ORDER{{
    LayerType::CurrentRun, LayerType::CommandLine, LayerType::Movie, LayerType::Netplay,
    LayerType::VRGame, LayerType::LocalGame, LayerType::GlobalGame, LayerType::Base,
}};
}
