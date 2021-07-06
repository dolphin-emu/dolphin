/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_MAX_VERTEX_ATTRIB_STRIDE 0x82E5
#define GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED 0x8221
#define GL_TEXTURE_BUFFER_BINDING 0x8C2A
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#define GL_CLIENT_STORAGE_BIT 0x0200
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000
#define GL_BUFFER_IMMUTABLE_STORAGE 0x821F
#define GL_BUFFER_STORAGE_FLAGS 0x8220
#define GL_CLEAR_TEXTURE 0x9365
#define GL_LOCATION_COMPONENT 0x934A
#define GL_TRANSFORM_FEEDBACK_BUFFER_INDEX 0x934B
#define GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE 0x934C
#define GL_QUERY_BUFFER 0x9192
#define GL_QUERY_BUFFER_BARRIER_BIT 0x00008000
#define GL_QUERY_BUFFER_BINDING 0x9193
#define GL_QUERY_RESULT_NO_WAIT 0x9194
#define GL_MIRROR_CLAMP_TO_EDGE 0x8743

typedef void(APIENTRYP PFNDOLBUFFERSTORAGEPROC)(GLenum target, GLsizeiptr size, const void* data,
                                                GLbitfield flags);
typedef void(APIENTRYP PFNDOLCLEARTEXIMAGEPROC)(GLuint texture, GLint level, GLenum format,
                                                GLenum type, const void* data);
typedef void(APIENTRYP PFNDOLCLEARTEXSUBIMAGEPROC)(GLuint texture, GLint level, GLint xoffset,
                                                   GLint yoffset, GLint zoffset, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLenum format,
                                                   GLenum type, const void* data);
typedef void(APIENTRYP PFNDOLBINDBUFFERSBASEPROC)(GLenum target, GLuint first, GLsizei count,
                                                  const GLuint* buffers);
typedef void(APIENTRYP PFNDOLBINDBUFFERSRANGEPROC)(GLenum target, GLuint first, GLsizei count,
                                                   const GLuint* buffers, const GLintptr* offsets,
                                                   const GLsizeiptr* sizes);
typedef void(APIENTRYP PFNDOLBINDTEXTURESPROC)(GLuint first, GLsizei count, const GLuint* textures);
typedef void(APIENTRYP PFNDOLBINDSAMPLERSPROC)(GLuint first, GLsizei count, const GLuint* samplers);
typedef void(APIENTRYP PFNDOLBINDIMAGETEXTURESPROC)(GLuint first, GLsizei count,
                                                    const GLuint* textures);
typedef void(APIENTRYP PFNDOLBINDVERTEXBUFFERSPROC)(GLuint first, GLsizei count,
                                                    const GLuint* buffers, const GLintptr* offsets,
                                                    const GLsizei* strides);

extern PFNDOLCLEARTEXIMAGEPROC dolClearTexImage;
extern PFNDOLCLEARTEXSUBIMAGEPROC dolClearTexSubImage;
extern PFNDOLBINDBUFFERSBASEPROC dolBindBuffersBase;
extern PFNDOLBINDBUFFERSRANGEPROC dolBindBuffersRange;
extern PFNDOLBINDTEXTURESPROC dolBindTextures;
extern PFNDOLBINDSAMPLERSPROC dolBindSamplers;
extern PFNDOLBINDIMAGETEXTURESPROC dolBindImageTextures;
extern PFNDOLBINDVERTEXBUFFERSPROC dolBindVertexBuffers;

#define glClearTexImage dolClearTexImage
#define glClearTexSubImage dolClearTexSubImage
#define glBindBuffersBase dolBindBuffersBase
#define glBindBuffersRange dolBindBuffersRange
#define glBindTextures dolBindTextures
#define glBindSamplers dolBindSamplers
#define glBindImageTextures dolBindImageTextures
#define glBindVertexBuffers dolBindVertexBuffers
