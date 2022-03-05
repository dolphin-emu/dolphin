// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

class GraphicsModAction
{
public:
  GraphicsModAction() = default;
  virtual ~GraphicsModAction() = default;
  GraphicsModAction(const GraphicsModAction&) = default;
  GraphicsModAction(GraphicsModAction&&) = default;
  GraphicsModAction& operator=(const GraphicsModAction&) = default;
  GraphicsModAction& operator=(GraphicsModAction&&) = default;

  virtual void OnDrawStarted(bool* skip) {}
  virtual void OnEFB(bool* skip, u32 texture_width, u32 texture_height, u32* scaled_width,
                     u32* scaled_height)
  {
  }
  virtual void OnXFB() {}
  virtual void OnProjection(Common::Matrix44* matrix) {}
  virtual void OnProjectionAndTexture(Common::Matrix44* matrix) {}
  virtual void OnTextureLoad() {}
  virtual void OnFrameEnd() {}
};
