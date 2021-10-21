/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
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
