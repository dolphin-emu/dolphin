// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/VertexManager.h"

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexShaderGen.h"

// Here's some global state. We only use this to keep track of what we've sent to the OpenGL state
// machine.

namespace OGL
{
std::unique_ptr<NativeVertexFormat>
Renderer::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<GLVertexFormat>(vtx_decl);
}

static inline GLuint VarToGL(VarType t)
{
  static const GLuint lookup[5] = {GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT, GL_SHORT,
                                   GL_FLOAT};
  return lookup[t];
}

static void SetPointer(u32 attrib, u32 stride, const AttributeFormat& format)
{
  if (!format.enable)
    return;

  glEnableVertexAttribArray(attrib);
  if (format.integer)
    glVertexAttribIPointer(attrib, format.components, VarToGL(format.type), stride,
                           (u8*)nullptr + format.offset);
  else
    glVertexAttribPointer(attrib, format.components, VarToGL(format.type), true, stride,
                          (u8*)nullptr + format.offset);
}

GLVertexFormat::GLVertexFormat(const PortableVertexDeclaration& vtx_decl)
    : NativeVertexFormat(vtx_decl)
{
  u32 vertex_stride = vtx_decl.stride;

  // We will not allow vertex components causing uneven strides.
  if (vertex_stride & 3)
    PanicAlert("Uneven vertex stride: %i", vertex_stride);

  VertexManager* const vm = static_cast<VertexManager*>(g_vertex_manager.get());

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);
  ProgramShaderCache::BindVertexFormat(this);

  // the element buffer is bound directly to the vao, so we must it set for every vao
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vm->GetIndexBufferHandle());
  glBindBuffer(GL_ARRAY_BUFFER, vm->GetVertexBufferHandle());

  SetPointer(SHADER_POSITION_ATTRIB, vertex_stride, vtx_decl.position);

  for (int i = 0; i < 3; i++)
    SetPointer(SHADER_NORM0_ATTRIB + i, vertex_stride, vtx_decl.normals[i]);

  for (int i = 0; i < 2; i++)
    SetPointer(SHADER_COLOR0_ATTRIB + i, vertex_stride, vtx_decl.colors[i]);

  for (int i = 0; i < 8; i++)
    SetPointer(SHADER_TEXTURE0_ATTRIB + i, vertex_stride, vtx_decl.texcoords[i]);

  SetPointer(SHADER_POSMTX_ATTRIB, vertex_stride, vtx_decl.posmtx);
}

GLVertexFormat::~GLVertexFormat()
{
  ProgramShaderCache::InvalidateVertexFormatIfBound(VAO);
  glDeleteVertexArrays(1, &VAO);
}
}  // namespace OGL
