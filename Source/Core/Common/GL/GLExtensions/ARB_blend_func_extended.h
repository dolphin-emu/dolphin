/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

#define GL_SRC1_COLOR 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC

typedef void(APIENTRYP PFNDOLBINDFRAGDATALOCATIONINDEXEDPROC)(GLuint program, GLuint colorNumber,
                                                              GLuint index, const GLchar* name);
typedef GLint(APIENTRYP PFNDOLGETFRAGDATAINDEXPROC)(GLuint program, const GLchar* name);

extern PFNDOLBINDFRAGDATALOCATIONINDEXEDPROC dolBindFragDataLocationIndexed;
extern PFNDOLGETFRAGDATAINDEXPROC dolGetFragDataIndex;

#define glBindFragDataLocationIndexed dolBindFragDataLocationIndexed
#define glGetFragDataIndex dolGetFragDataIndex
