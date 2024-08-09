// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

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
  virtual void OnLight(GraphicsModActionData::Light*) {}
  virtual void OnFrameEnd() {}

  void SetID(u64 id) { m_id = id; }
  u64 GetID() const { return m_id; }

  virtual void DrawImGui() {}
  virtual void SerializeToConfig(picojson::object* obj) {}
  virtual std::string GetFactoryName() const { return ""; }

private:
  u64 m_id = 0;
};
