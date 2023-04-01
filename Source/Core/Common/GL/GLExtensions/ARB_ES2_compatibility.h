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

typedef void (APIENTRYP PFNDOLRELEASESHADERCOMPILERPROC) (void);
typedef void (APIENTRYP PFNDOLSHADERBINARYPROC) (GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length);
typedef void (APIENTRYP PFNDOLGETSHADERPRECISIONFORMATPROC) (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (APIENTRYP PFNDOLDEPTHRANGEFPROC) (GLfloat n, GLfloat f);
typedef void (APIENTRYP PFNDOLCLEARDEPTHFPROC) (GLfloat d);

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
