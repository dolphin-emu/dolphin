// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

enum class EFBAccessType;
enum class EFBReinterpretType;

struct EfbPokeData
{
  u16 x, y;
  u32 data;
};

// Renderer really isn't a very good name for this class - it's more like "Misc".
// It used to be a massive mess, but almost everything has been refactored out.
//
// All that's left is a thin abstraction layer for VideoSoftware to intercept EFB accesses.
class Renderer
{
public:
  virtual ~Renderer();

  virtual void ReinterpretPixelData(EFBReinterpretType convtype);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points);
};

extern std::unique_ptr<Renderer> g_renderer;
