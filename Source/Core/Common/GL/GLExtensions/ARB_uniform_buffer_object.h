/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLGETINTEGERI_VPROC)(GLenum target, GLuint index, GLint* data);
typedef void(APIENTRYP PFNDOLBINDBUFFERRANGEPROC)(GLenum target, GLuint index, GLuint buffer,
                                                  GLintptr offset, GLsizeiptr size);
typedef void(APIENTRYP PFNDOLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void(APIENTRYP PFNDOLGETUNIFORMINDICESPROC)(GLuint program, GLsizei uniformCount,
                                                    const GLchar* const* uniformNames,
                                                    GLuint* uniformIndices);
typedef void(APIENTRYP PFNDOLGETACTIVEUNIFORMSIVPROC)(GLuint program, GLsizei uniformCount,
                                                      const GLuint* uniformIndices, GLenum pname,
                                                      GLint* params);
typedef void(APIENTRYP PFNDOLGETACTIVEUNIFORMNAMEPROC)(GLuint program, GLuint uniformIndex,
                                                       GLsizei bufSize, GLsizei* length,
                                                       GLchar* uniformName);
typedef GLuint(APIENTRYP PFNDOLGETUNIFORMBLOCKINDEXPROC)(GLuint program,
                                                         const GLchar* uniformBlockName);
typedef void(APIENTRYP PFNDOLGETACTIVEUNIFORMBLOCKIVPROC)(GLuint program, GLuint uniformBlockIndex,
                                                          GLenum pname, GLint* params);
typedef void(APIENTRYP PFNDOLGETACTIVEUNIFORMBLOCKNAMEPROC)(GLuint program,
                                                            GLuint uniformBlockIndex,
                                                            GLsizei bufSize, GLsizei* length,
                                                            GLchar* uniformBlockName);
typedef void(APIENTRYP PFNDOLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex,
                                                      GLuint uniformBlockBinding);

extern PFNDOLBINDBUFFERBASEPROC dolBindBufferBase;
extern PFNDOLBINDBUFFERRANGEPROC dolBindBufferRange;
extern PFNDOLGETACTIVEUNIFORMBLOCKNAMEPROC dolGetActiveUniformBlockName;
extern PFNDOLGETACTIVEUNIFORMBLOCKIVPROC dolGetActiveUniformBlockiv;
extern PFNDOLGETACTIVEUNIFORMNAMEPROC dolGetActiveUniformName;
extern PFNDOLGETACTIVEUNIFORMSIVPROC dolGetActiveUniformsiv;
extern PFNDOLGETINTEGERI_VPROC dolGetIntegeri_v;
extern PFNDOLGETUNIFORMBLOCKINDEXPROC dolGetUniformBlockIndex;
extern PFNDOLGETUNIFORMINDICESPROC dolGetUniformIndices;
extern PFNDOLUNIFORMBLOCKBINDINGPROC dolUniformBlockBinding;

#define glBindBufferBase dolBindBufferBase
#define glBindBufferRange dolBindBufferRange
#define glGetActiveUniformBlockName dolGetActiveUniformBlockName
#define glGetActiveUniformBlockiv dolGetActiveUniformBlockiv
#define glGetActiveUniformName dolGetActiveUniformName
#define glGetActiveUniformsiv dolGetActiveUniformsiv
#define glGetIntegeri_v dolGetIntegeri_v
#define glGetUniformBlockIndex dolGetUniformBlockIndex
#define glGetUniformIndices dolGetUniformIndices
#define glUniformBlockBinding dolUniformBlockBinding
