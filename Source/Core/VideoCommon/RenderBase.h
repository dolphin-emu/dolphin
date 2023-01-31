// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later


#pragma once

#include <memory>
#include <tuple>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VideoEvents.h"

class PointerWrap;
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
// All that's left is Widescreen tracking, pixel format and a thin abstraction layer
// for VideoSoftware to intercept EFB accesses.
class Renderer
{
public:
  Renderer();
  virtual ~Renderer();

  virtual void ReinterpretPixelData(EFBReinterpretType convtype);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points);

  bool IsGameWidescreen() const { return m_is_game_widescreen; }

  PixelFormat GetPrevPixelFormat() const { return m_prev_efb_format; }
  void StorePixelFormat(PixelFormat new_format) { m_prev_efb_format = new_format; }

  static bool UseVertexDepthRange();
  void DoState(PointerWrap& p);

  void OnConfigChanged(u32 bits);

protected:
  void UpdateWidescreen();
  void UpdateWidescreenHeuristic();

  bool m_is_game_widescreen = false;
  bool m_was_orthographically_anamorphic = false;

private:
  PixelFormat m_prev_efb_format;

  EventHook m_update_widescreen_handle;
  EventHook m_config_changed_handle;
};

extern std::unique_ptr<Renderer> g_renderer;
