/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
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
