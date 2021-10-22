// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

using BBoxType = s32;
constexpr u32 NUM_BBOX_VALUES = 4;

class BoundingBox
{
public:
  explicit BoundingBox() = default;
  virtual ~BoundingBox() = default;

  bool IsEnabled() const { return m_is_active; }
  void Enable();
  void Disable();

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
};
