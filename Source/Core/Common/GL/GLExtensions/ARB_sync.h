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

typedef GLsync(APIENTRYP PFNDOLFENCESYNCPROC)(GLenum condition, GLbitfield flags);
typedef GLboolean(APIENTRYP PFNDOLISSYNCPROC)(GLsync sync);
typedef void(APIENTRYP PFNDOLDELETESYNCPROC)(GLsync sync);
typedef GLenum(APIENTRYP PFNDOLCLIENTWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void(APIENTRYP PFNDOLWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void(APIENTRYP PFNDOLGETINTEGER64VPROC)(GLenum pname, GLint64* data);
typedef void(APIENTRYP PFNDOLGETSYNCIVPROC)(GLsync sync, GLenum pname, GLsizei bufSize,
                                            GLsizei* length, GLint* values);
typedef void(APIENTRYP PFNDOLGETINTEGER64I_VPROC)(GLenum target, GLuint index, GLint64* data);

extern PFNDOLCLIENTWAITSYNCPROC dolClientWaitSync;
extern PFNDOLDELETESYNCPROC dolDeleteSync;
extern PFNDOLFENCESYNCPROC dolFenceSync;
extern PFNDOLGETINTEGER64VPROC dolGetInteger64v;
extern PFNDOLGETSYNCIVPROC dolGetSynciv;
extern PFNDOLISSYNCPROC dolIsSync;
extern PFNDOLWAITSYNCPROC dolWaitSync;

#define glClientWaitSync dolClientWaitSync
#define glDeleteSync dolDeleteSync
#define glFenceSync dolFenceSync
#define glGetInteger64v dolGetInteger64v
#define glGetSynciv dolGetSynciv
#define glIsSync dolIsSync
#define glWaitSync dolWaitSync
