/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
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
