// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#ifndef GLFUNCTIONS_H_
#define GLFUNCTIONS_H_
#include "GLInterface.h"

#ifdef USE_GLES3
typedef GLvoid* (*PFNGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLvoid* (*PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (*PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef GLboolean (*PFNGLUNMAPBUFFERPROC) (GLenum target);

typedef void (*PFNGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
// VAOS
typedef void (*PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint* arrays);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint* arrays);
typedef void (*PFNGLBINDVERTEXARRAYPROC) (GLuint array);

// Sync
typedef GLenum (*PFNGLCLIENTWAITSYNCPROC) (GLsync GLsync,GLbitfield flags,GLuint64 timeout);
typedef void (*PFNGLDELETESYNCPROC) (GLsync GLsync);
typedef GLsync (*PFNGLFENCESYNCPROC) (GLenum condition,GLbitfield flags);

//Sampler
typedef void (*PFNGLSAMPLERPARAMETERFPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (*PFNGLSAMPLERPARAMETERIPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (*PFNGLSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, const GLfloat* params);
typedef void (*PFNGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
typedef void (*PFNGLDELETESAMPLERSPROC) (GLsizei count, const GLuint * samplers);
typedef void (*PFNGLGENSAMPLERSPROC) (GLsizei count, GLuint* samplers);

//Program binar
typedef void (*PFNGLGETPROGRAMBINARYPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLenum *binaryFormat, GLvoid*binary);
typedef void (*PFNGLPROGRAMBINARYPROC) (GLuint program, GLenum binaryFormat, const void* binary, GLsizei length);
typedef void (*PFNGLPROGRAMPARAMETERIPROC) (GLuint program, GLenum pname, GLint value);

typedef GLuint (*PFNGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const GLchar* uniformBlockName);
typedef void (*PFNGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

//Query
typedef void (*PFNGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void (*PFNGLENDQUERYPROC) (GLenum target);
typedef void (*PFNGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint* params);
typedef void (*PFNGLDELETEQUERIESPROC) (GLsizei n, const GLuint* ids);
typedef void (*PFNGLGENQUERIESPROC) (GLsizei n, GLuint* ids);

// glDraw*
typedef void (*PFNGLDRAWRANGEELEMENTSPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid* indices);

// ptrs
extern PFNGLBEGINQUERYPROC glBeginQuery;
extern PFNGLENDQUERYPROC glEndQuery;
extern PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
extern PFNGLDELETEQUERIESPROC glDeleteQueries;
extern PFNGLGENQUERIESPROC glGenQueries;

extern PFNGLUNMAPBUFFERPROC glUnmapBuffer;
extern PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
extern PFNGLBINDBUFFERRANGEPROC glBindBufferRange;

extern PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;

extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

extern PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
extern PFNGLDELETESYNCPROC glDeleteSync;
extern PFNGLFENCESYNCPROC glFenceSync;

extern PFNGLGETPROGRAMBINARYPROC glGetProgramBinary;
extern PFNGLPROGRAMBINARYPROC glProgramBinary;
extern PFNGLPROGRAMPARAMETERIPROC glProgramParameteri;

extern PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;

//Sampler
extern PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
extern PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
extern PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
extern PFNGLBINDSAMPLERPROC glBindSampler;
extern PFNGLDELETESAMPLERSPROC glDeleteSamplers;
extern PFNGLGENSAMPLERSPROC glGenSamplers;

extern PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
extern PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
#endif

namespace GLFunc
{
	void Init();
}
#endif
