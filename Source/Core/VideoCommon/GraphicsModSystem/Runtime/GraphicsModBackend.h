// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string_view>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoEvents.h"

class GraphicsModAction;
namespace GraphicsModSystem::Runtime
{
class GraphicsModBackend
{
public:
  virtual ~GraphicsModBackend() = default;

  virtual void OnDraw(const DrawDataView& draw_started, VertexManagerBase* vertex_manager) = 0;
  virtual void OnTextureUnload(TextureType texture_type, std::string_view texture_hash) = 0;
  virtual void OnTextureLoad(const TextureView& texture) = 0;
  virtual void OnTextureCreate(const TextureView& texture) = 0;
  virtual void OnLight() = 0;
  virtual void OnFramePresented(const PresentInfo& present_info) = 0;

  virtual void AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices) = 0;
  virtual void ResetIndices() = 0;

protected:
  void CustomDraw(const DrawDataView& draw_data, VertexManagerBase* vertex_manager,
                  std::span<GraphicsModAction*> actions);

  // This will ensure gpu skinned draw calls are joined together
  DrawCallID GetSkinnedDrawCallID(DrawCallID draw_call_id, MaterialID material_id,
                                  const DrawDataView& draw_data);

private:
  bool m_last_draw_gpu_skinned = false;
  GraphicsModSystem::DrawCallID m_last_draw_call_id = GraphicsModSystem::DrawCallID::INVALID;
  GraphicsModSystem::MaterialID m_last_material_id = GraphicsModSystem::MaterialID::INVALID;
};
}  // namespace GraphicsModSystem::Runtime
