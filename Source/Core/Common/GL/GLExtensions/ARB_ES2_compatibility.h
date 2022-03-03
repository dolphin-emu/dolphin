/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLRELEASESHADERCOMPILERPROC)(void);
typedef void(APIENTRYP PFNDOLSHADERBINARYPROC)(GLsizei count, const GLuint* shaders,
                                               GLenum binaryformat, const void* binary,
                                               GLsizei length);
typedef void(APIENTRYP PFNDOLGETSHADERPRECISIONFORMATPROC)(GLenum shadertype, GLenum precisiontype,
                                                           GLint* range, GLint* precision);
typedef void(APIENTRYP PFNDOLDEPTHRANGEFPROC)(GLfloat n, GLfloat f);
typedef void(APIENTRYP PFNDOLCLEARDEPTHFPROC)(GLfloat d);

extern PFNDOLCLEARDEPTHFPROC dolClearDepthf;
extern PFNDOLDEPTHRANGEFPROC dolDepthRangef;
extern PFNDOLGETSHADERPRECISIONFORMATPROC dolGetShaderPrecisionFormat;
extern PFNDOLRELEASESHADERCOMPILERPROC dolReleaseShaderCompiler;
extern PFNDOLSHADERBINARYPROC dolShaderBinary;

#define glClearDepthf dolClearDepthf
#define glDepthRangef dolDepthRangef
#define glGetShaderPrecisionFormat dolGetShaderPrecisionFormat
#define glReleaseShaderCompiler dolReleaseShaderCompiler
#define glShaderBinary dolShaderBinary
