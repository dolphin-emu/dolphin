// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionData.h"

class GraphicsModAction
{
public:
  GraphicsModAction() = default;
  virtual ~GraphicsModAction() = default;
  GraphicsModAction(const GraphicsModAction&) = default;
  GraphicsModAction(GraphicsModAction&&) = default;
  GraphicsModAction& operator=(const GraphicsModAction&) = default;
  GraphicsModAction& operator=(GraphicsModAction&&) = default;

  virtual void OnDrawStarted(GraphicsModActionData::DrawStarted*) {}
  virtual void OnEFB(GraphicsModActionData::EFB*) {}
  virtual void OnXFB() {}
  virtual void OnProjection(GraphicsModActionData::Projection*) {}
  virtual void OnProjectionAndTexture(GraphicsModActionData::Projection*) {}
  virtual void OnTextureLoad(GraphicsModActionData::TextureLoad*) {}
  virtual void OnTextureCreate(GraphicsModActionData::TextureCreate*) {}
  virtual void OnFrameEnd() {}
};
