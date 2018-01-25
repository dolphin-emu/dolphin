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

  void CreateDeviceObjects() override;
  void DestroyDeviceObjects() override;

  GLuint GetVertexBufferHandle() const;
  GLuint GetIndexBufferHandle() const;

protected:
  void ResetBuffer(u32 stride) override;

private:
  void Draw(u32 stride);
  void vFlush() override;
  void vFlush3D(bool useDstAlpha);
  void PrepareDrawBuffers(u32 stride);

  GLuint m_vertex_buffers;
  GLuint m_index_buffers;

  // Alternative buffers in CPU memory for primatives we are going to discard.
  std::vector<u8> m_cpu_v_buffer;
  std::vector<u16> m_cpu_i_buffer;
};
}
