// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

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
class VertexManager : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager();

  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;

  void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size) override;

  GLuint GetVertexBufferHandle() const;
  GLuint GetIndexBufferHandle() const;

protected:
  void CreateDeviceObjects() override;
  void DestroyDeviceObjects() override;
  void ResetBuffer(u32 vertex_stride, bool cull_all) override;
  void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices, u32* out_base_vertex,
                    u32* out_base_index) override;
  void UploadConstants() override;
  void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex) override;

private:
  std::unique_ptr<StreamBuffer> m_vertex_buffer;
  std::unique_ptr<StreamBuffer> m_index_buffer;

  // Alternative buffers in CPU memory for primatives we are going to discard.
  std::vector<u8> m_cpu_v_buffer;
  std::vector<u16> m_cpu_i_buffer;
};
}  // namespace OGL
