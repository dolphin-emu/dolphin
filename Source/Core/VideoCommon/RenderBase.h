// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The GPU draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/MathUtil.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VideoEvents.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;
class AbstractStagingTexture;
class NativeVertexFormat;
class PixelShaderManager;
class PointerWrap;
struct ComputePipelineConfig;
struct AbstractPipelineConfig;
struct PortableVertexDeclaration;
struct TextureConfig;
enum class AbstractTextureFormat : u32;
enum class ShaderStage;
enum class EFBAccessType;
enum class EFBReinterpretType;
enum class StagingTextureType;

namespace VideoCommon
{
class AsyncShaderCompiler;
}

struct EfbPokeData
{
  u16 x, y;
  u32 data;
};

// Renderer really isn't a very good name for this class - it's more like "Misc".
// The long term goal is to get rid of this class and replace it with others that make
// more sense.
class Renderer
{
public:
  Renderer();
  virtual ~Renderer();

  // Ideal internal resolution - multiple of the native EFB resolution
  int GetTargetWidth() const { return m_target_width; }
  int GetTargetHeight() const { return m_target_height; }

  // EFB coordinate conversion functions
  // Use this to convert a whole native EFB rect to backbuffer coordinates
  MathUtil::Rectangle<int> ConvertEFBRectangle(const MathUtil::Rectangle<int>& rc) const;

  unsigned int GetEFBScale() const;

  // Use this to upscale native EFB coordinates to IDEAL internal resolution
  int EFBToScaledX(int x) const;
  int EFBToScaledY(int y) const;

  // Floating point versions of the above - only use them if really necessary
  float EFBToScaledXf(float x) const;
  float EFBToScaledYf(float y) const;

  void ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                   bool zEnable, u32 color, u32 z);
  virtual void ReinterpretPixelData(EFBReinterpretType convtype);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points);

  // Track swaps for save-states
  void TrackSwaps(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks);

  bool IsGameWidescreen() const { return m_is_game_widescreen; }

  PixelFormat GetPrevPixelFormat() const { return m_prev_efb_format; }
  void StorePixelFormat(PixelFormat new_format) { m_prev_efb_format = new_format; }

  bool UseVertexDepthRange() const;
  void DoState(PointerWrap& p);

  bool CalculateTargetSize();

  int m_frame_count = 0;

  void OnConfigChanged(u32 bits);

protected:
  void UpdateWidescreen();
  void UpdateWidescreenHeuristic();

  std::tuple<int, int> CalculateTargetScale(int x, int y) const;

  bool m_is_game_widescreen = false;
  bool m_was_orthographically_anamorphic = false;

  // The framebuffer size
  int m_target_width = 1;
  int m_target_height = 1;

private:
  PixelFormat m_prev_efb_format;
  unsigned int m_efb_scale = 1;

  u64 m_last_xfb_ticks = 0;
  u32 m_last_xfb_addr = 0;
  u32 m_last_xfb_width = 0;
  u32 m_last_xfb_stride = 0;
  u32 m_last_xfb_height = 0;

  EventHook m_update_widescreen_handle;
  EventHook m_config_changed_handle;
};

extern std::unique_ptr<Renderer> g_renderer;
