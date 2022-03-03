// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexManagerBase.h"

namespace OGL
{
class StreamBuffer;
class GLVertexFormat : public NativeVertexFormat
{
public:
  GLVertexFormat(const PortableVertexDeclaration& vtx_decl);
  ~GLVertexFormat();

  GLuint VAO;
};

// Handles the OpenGL details of drawing lots of vertices quickly.
// Other functionality is moving out.
class VertexManager final : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager() override;

  bool Initialize() override;

  void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size) override;
  bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                         u32* out_offset) override;
  bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format, u32* out_offset,
                         const void* palette_data, u32 palette_size,
                         TexelBufferFormat palette_format, u32* out_palette_offset) override;

  GLuint GetVertexBufferHandle() const;
  GLuint GetIndexBufferHandle() const;

protected:
  void ResetBuffer(u32 vertex_stride) override;
  void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices, u32* out_base_vertex,
                    u32* out_base_index) override;
  void UploadUniforms() override;

private:
  std::unique_ptr<StreamBuffer> m_vertex_buffer;
  std::unique_ptr<StreamBuffer> m_index_buffer;
  std::unique_ptr<StreamBuffer> m_texel_buffer;
  std::array<GLuint, NUM_TEXEL_BUFFER_FORMATS> m_texel_buffer_views{};
};
}  // namespace OGL
