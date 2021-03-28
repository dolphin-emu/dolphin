// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"
#include "VideoCommon/VertexManagerBase.h"

namespace Vulkan
{
class StreamBuffer;

class VertexManager : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager();

  bool Initialize() override;

  void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size) override;
  bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                         u32* out_offset) override;
  bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format, u32* out_offset,
                         const void* palette_data, u32 palette_size,
                         TexelBufferFormat palette_format, u32* out_palette_offset) override;

protected:
  void ResetBuffer(u32 vertex_stride) override;
  void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices, u32* out_base_vertex,
                    u32* out_base_index) override;
  void UploadUniforms() override;

  void DestroyTexelBufferViews();

  void UpdateVertexShaderConstants();
  void UpdateGeometryShaderConstants();
  void UpdatePixelShaderConstants();

  // Allocates storage in the uniform buffer of the specified size. If this storage cannot be
  // allocated immediately, the current command buffer will be submitted and all stage's
  // constants will be re-uploaded. false will be returned in this case, otherwise true.
  bool ReserveConstantStorage();
  void UploadAllConstants();

  std::unique_ptr<StreamBuffer> m_vertex_stream_buffer;
  std::unique_ptr<StreamBuffer> m_index_stream_buffer;
  std::unique_ptr<StreamBuffer> m_uniform_stream_buffer;
  std::unique_ptr<StreamBuffer> m_texel_stream_buffer;
  std::array<VkBufferView, NUM_TEXEL_BUFFER_FORMATS> m_texel_buffer_views = {};
  u32 m_uniform_buffer_reserve_size = 0;
};
}  // namespace Vulkan
