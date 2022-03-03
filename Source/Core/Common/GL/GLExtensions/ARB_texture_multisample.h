/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void(APIENTRYP PFNDOLTEXIMAGE2DMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height,
                                                        GLboolean fixedsamplelocations);
typedef void(APIENTRYP PFNDOLTEXIMAGE3DMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height, GLsizei depth,
                                                        GLboolean fixedsamplelocations);
typedef void(APIENTRYP PFNDOLGETMULTISAMPLEFVPROC)(GLenum pname, GLuint index, GLfloat* val);
typedef void(APIENTRYP PFNDOLSAMPLEMASKIPROC)(GLuint maskNumber, GLbitfield mask);

extern PFNDOLTEXIMAGE2DMULTISAMPLEPROC dolTexImage2DMultisample;
extern PFNDOLTEXIMAGE3DMULTISAMPLEPROC dolTexImage3DMultisample;
extern PFNDOLGETMULTISAMPLEFVPROC dolGetMultisamplefv;
extern PFNDOLSAMPLEMASKIPROC dolSampleMaski;

#define glTexImage2DMultisample dolTexImage2DMultisample
#define glTexImage3DMultisample dolTexImage3DMultisample
#define glGetMultisamplefv dolGetMultisamplefv
#define glSampleMaski dolSampleMaski
