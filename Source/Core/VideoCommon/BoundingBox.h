// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

class PixelShaderManager;
class PointerWrap;

using BBoxType = s32;
constexpr u32 NUM_BBOX_VALUES = 4;

class BoundingBox
{
public:
  explicit BoundingBox() = default;
  virtual ~BoundingBox() = default;

  bool IsEnabled() const { return m_is_active; }
  void Enable(PixelShaderManager& pixel_shader_manager);
  void Disable(PixelShaderManager& pixel_shader_manager);

  void Flush();

  u16 Get(u32 index);
  void Set(u32 index, u16 value);

  void DoState(PointerWrap& p);

  // Initialize, Read, and Write are only safe to call if the backend supports bounding box,
  // otherwise unexpected exceptions can occur
  virtual bool Initialize() = 0;

protected:
  virtual std::vector<BBoxType> Read(u32 index, u32 length) = 0;
  // TODO: This can likely use std::span once we're on C++20
  virtual void Write(u32 index, const std::vector<BBoxType>& values) = 0;

private:
  void Readback();

  bool m_is_active = false;

  std::array<BBoxType, NUM_BBOX_VALUES> m_values = {};
  std::array<bool, NUM_BBOX_VALUES> m_dirty = {};
  bool m_is_valid = true;

  // Nintendo's SDK seems to write "default" bounding box values before every draw (1023 0 1023 0
  // are the only values encountered so far, which happen to be the extents allowed by the BP
  // registers) to reset the registers for comparison in the pixel engine, and presumably to detect
  // whether GX has updated the registers with real values.
  //
  // We can store these values when Bounding Box emulation is disabled and return them on read,
  // which the game will interpret as "no pixels have been drawn"
  //
  // This produces much better results than just returning garbage, which can cause games like
  // Ultimate Spider-Man to crash
  std::array<u16, 4> m_bounding_box_fallback = {};
};

extern std::unique_ptr<BoundingBox> g_bounding_box;
