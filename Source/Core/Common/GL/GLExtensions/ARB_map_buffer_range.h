/*
** Copyright (c) 2013-2015 The Khronos Group Inc.
** SPDX-License-Identifier: MIT
*/

#include "Common/GL/GLExtensions/gl_common.h"

typedef void*(APIENTRYP PFNDOLMAPBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length,
                                                  GLbitfield access);
typedef void(APIENTRYP PFNDOLFLUSHMAPPEDBUFFERRANGEPROC)(GLenum target, GLintptr offset,
                                                         GLsizeiptr length);

extern PFNDOLFLUSHMAPPEDBUFFERRANGEPROC dolFlushMappedBufferRange;
extern PFNDOLMAPBUFFERRANGEPROC dolMapBufferRange;

#define glFlushMappedBufferRange dolFlushMappedBufferRange
#define glMapBufferRange dolMapBufferRange
