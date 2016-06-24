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
