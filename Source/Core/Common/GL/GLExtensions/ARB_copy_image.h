/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLCOPYIMAGESUBDATAPROC)(GLuint srcName, GLenum srcTarget, GLint srcLevel,
                                                   GLint srcX, GLint srcY, GLint srcZ,
                                                   GLuint dstName, GLenum dstTarget, GLint dstLevel,
                                                   GLint dstX, GLint dstY, GLint dstZ,
                                                   GLsizei srcWidth, GLsizei srcHeight,
                                                   GLsizei srcDepth);

extern PFNDOLCOPYIMAGESUBDATAPROC dolCopyImageSubData;

#define glCopyImageSubData dolCopyImageSubData
