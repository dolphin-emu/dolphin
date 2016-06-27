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

#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0x8257
#define GL_PROGRAM_BINARY_LENGTH 0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS 0x87FE
#define GL_PROGRAM_BINARY_FORMATS 0x87FF

typedef void(APIENTRYP PFNDOLGETPROGRAMBINARYPROC)(GLuint program, GLsizei bufSize, GLsizei* length,
                                                   GLenum* binaryFormat, void* binary);
typedef void(APIENTRYP PFNDOLPROGRAMBINARYPROC)(GLuint program, GLenum binaryFormat,
                                                const void* binary, GLsizei length);
typedef void(APIENTRYP PFNDOLPROGRAMPARAMETERIPROC)(GLuint program, GLenum pname, GLint value);

extern PFNDOLGETPROGRAMBINARYPROC dolGetProgramBinary;
extern PFNDOLPROGRAMBINARYPROC dolProgramBinary;
extern PFNDOLPROGRAMPARAMETERIPROC dolProgramParameteri;

#define glGetProgramBinary dolGetProgramBinary
#define glProgramBinary dolProgramBinary
#define glProgramParameteri dolProgramParameteri
