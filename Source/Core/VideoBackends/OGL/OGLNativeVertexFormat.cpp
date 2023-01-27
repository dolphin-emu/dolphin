// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/GL/GLUtil.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoBackends/OGL/OGLVertexManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexShaderGen.h"

// Here's some global state. We only use this to keep track of what we've sent to the OpenGL state
// machine.

namespace OGL
{
std::unique_ptr<NativeVertexFormat>
OGLGfx::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<GLVertexFormat>(vtx_decl);
}

static inline GLuint VarToGL(ComponentFormat t)
{
  static constexpr Common::EnumMap<GLuint, ComponentFormat::Float> lookup = {
      GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT, GL_SHORT, GL_FLOAT,
  };
  return lookup[t];
}

static void SetPointer(ShaderAttrib attrib, u32 stride, const AttributeFormat& format)
{
  if (!format.enable)
    return;

  glEnableVertexAttribArray(static_cast<GLuint>(attrib));
  if (format.integer)
    glVertexAttribIPointer(static_cast<GLuint>(attrib), format.components, VarToGL(format.type),
                           stride, (u8*)nullptr + format.offset);
  else
    glVertexAttribPointer(static_cast<GLuint>(attrib), format.components, VarToGL(format.type),
                          true, stride, (u8*)nullptr + format.offset);
}

GLVertexFormat::GLVertexFormat(const PortableVertexDeclaration& vtx_decl)
    : NativeVertexFormat(vtx_decl)
{
  const u32 vertex_stride = vtx_decl.stride;

  // We will not allow vertex components causing uneven strides.
  if (vertex_stride & 3)
    PanicAlertFmt("Uneven vertex stride: {}", vertex_stride);

  VertexManager* const vm = static_cast<VertexManager*>(g_vertex_manager.get());

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  // the element buffer is bound directly to the vao, so we must it set for every vao
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vm->GetIndexBufferHandle());
  glBindBuffer(GL_ARRAY_BUFFER, vm->GetVertexBufferHandle());

  SetPointer(ShaderAttrib::Position, vertex_stride, vtx_decl.position);

  for (u32 i = 0; i < 3; i++)
    SetPointer(ShaderAttrib::Normal + i, vertex_stride, vtx_decl.normals[i]);

  for (u32 i = 0; i < 2; i++)
    SetPointer(ShaderAttrib::Color0 + i, vertex_stride, vtx_decl.colors[i]);

  for (u32 i = 0; i < 8; i++)
    SetPointer(ShaderAttrib::TexCoord0 + i, vertex_stride, vtx_decl.texcoords[i]);

  SetPointer(ShaderAttrib::PositionMatrix, vertex_stride, vtx_decl.posmtx);

  // Other code shouldn't have to worry about its vertex formats being randomly unbound
  ProgramShaderCache::ReBindVertexFormat();
}

GLVertexFormat::~GLVertexFormat()
{
  ProgramShaderCache::InvalidateVertexFormatIfBound(VAO);
  glDeleteVertexArrays(1, &VAO);
}
}  // namespace OGL
