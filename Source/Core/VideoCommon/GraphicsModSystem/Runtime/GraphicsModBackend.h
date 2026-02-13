// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string_view>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CameraManager.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/ShaderGenCommon.h"
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
  virtual void OnFramePresented(const PresentInfo& present_info);

  virtual void AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices) = 0;
  virtual void ResetIndices() = 0;

  void SetHostConfig(const ShaderHostConfig& config);

  void ClearAdditionalCameras(const MathUtil::Rectangle<int>& rc, bool color_enable,
                              bool alpha_enable, bool z_enable, u32 color, u32 z);

protected:
  void CustomDraw(const DrawDataView& draw_data, VertexManagerBase* vertex_manager,
                  std::span<GraphicsModAction*> actions, DrawCallID draw_call);

  // This will ensure gpu skinned draw calls are joined together
  DrawCallID GetSkinnedDrawCallID(DrawCallID draw_call_id, MaterialID material_id,
                                  const DrawDataView& draw_data);

  static bool IsDrawGPUSkinned(NativeVertexFormat* format, PrimitiveType primitive_type);

  ShaderHostConfig m_shader_host_config;

  VideoCommon::CameraManager m_camera_manager;

private:
  bool m_last_draw_gpu_skinned = false;
  GraphicsModSystem::DrawCallID m_last_draw_call_id = GraphicsModSystem::DrawCallID::INVALID;
  GraphicsModSystem::MaterialID m_last_material_id = GraphicsModSystem::MaterialID::INVALID;
};
}  // namespace GraphicsModSystem::Runtime
