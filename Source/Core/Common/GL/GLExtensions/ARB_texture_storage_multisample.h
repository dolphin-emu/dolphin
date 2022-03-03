/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLTEXSTORAGE2DMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                          GLenum internalformat, GLsizei width,
                                                          GLsizei height,
                                                          GLboolean fixedsamplelocations);
typedef void(APIENTRYP PFNDOLTEXSTORAGE3DMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                          GLenum internalformat, GLsizei width,
                                                          GLsizei height, GLsizei depth,
                                                          GLboolean fixedsamplelocations);

extern PFNDOLTEXSTORAGE2DMULTISAMPLEPROC dolTexStorage2DMultisample;
extern PFNDOLTEXSTORAGE3DMULTISAMPLEPROC dolTexStorage3DMultisample;

#define glTexStorage2DMultisample dolTexStorage2DMultisample
#define glTexStorage3DMultisample dolTexStorage3DMultisample
