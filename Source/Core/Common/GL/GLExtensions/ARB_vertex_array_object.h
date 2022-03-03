/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLBINDVERTEXARRAYPROC)(GLuint array);
typedef void(APIENTRYP PFNDOLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void(APIENTRYP PFNDOLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef GLboolean(APIENTRYP PFNDOLISVERTEXARRAYPROC)(GLuint array);

extern PFNDOLBINDVERTEXARRAYPROC dolBindVertexArray;
extern PFNDOLDELETEVERTEXARRAYSPROC dolDeleteVertexArrays;
extern PFNDOLGENVERTEXARRAYSPROC dolGenVertexArrays;
extern PFNDOLISVERTEXARRAYPROC dolIsVertexArray;

#define glBindVertexArray dolBindVertexArray
#define glDeleteVertexArrays dolDeleteVertexArrays
#define glGenVertexArrays dolGenVertexArrays
#define glIsVertexArray dolIsVertexArray
